package com.dashu.open.audioplay;

import com.dashu.open.audio.core.NativeMP3Decoder;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;

public class StreamPlayer {

	public static final int StreamPlayerError = -1;
	public static final int StreamPlayerPlaying = 0x104;
	public static final int StreamPlayerPaused = 0x105;
	public static final int StreamPlayerStopped = 0x106;
	public static final int StreamPlayerTitle = 0x107;
	public static final int StreamPlayerPosition = 0x108;
	public static final int StreamPlayerTime = 0x109;

	private static final int STATUS_PLAYING = 1;
	private static final int STATUS_STOPING = 2;
	private static final int STATUS_STOPED = 3;
	
	public static final String TAG = "Dashu";
	private PlayerFeed feed;
	private Context context;
	private Handler handler;
	private short[] audioBuffer;
	private int mPlayFlag = STATUS_STOPED;
	private int mAudioMinBufSize;
	private NativeMP3Decoder MP3Decoder;
	private AudioTrack mAudioTrack;

	public StreamPlayer(Context context, Handler handler) {
		this.context = context;
		this.handler = handler;
		audioBuffer = new short[1024 * 128];
		MP3Decoder = new NativeMP3Decoder();
		feed = new PlayerFeed();
	}

	public void stop() {
		Log.i(TAG, "Stop in StreamPlayer");
		if (mPlayFlag == STATUS_PLAYING){
			mPlayFlag = STATUS_STOPING;// 音频线程关闭
			while (mPlayFlag != STATUS_STOPED) {
				try {
					Thread.sleep(20);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
			mAudioTrack.stop();
			mAudioTrack.release();// 关闭并释放资源
			MP3Decoder.closeAduioPlayer();
		}
	}

	public void start(final String url) {
		if (mPlayFlag == STATUS_PLAYING) {
			Toast.makeText(context, "have already start", Toast.LENGTH_SHORT)
					.show();
			return;
		}
		Log.i(TAG, "Start in StreamPlayer");
		int ret = MP3Decoder.initAudioPlayer(url, feed);
		if (ret < 0) {
			handler.sendEmptyMessage(StreamPlayerError);
			return;
		}

		new Thread() {
			public void run() {
				playing();
			};
		}.start();
	}

	private void playing() {

		int ret = initAudioPlayer();
		if (ret < 0) {
			handler.sendEmptyMessage(StreamPlayerError);
			return;
		}
		
		mPlayFlag = STATUS_PLAYING;
		mAudioTrack.play();
		handler.sendEmptyMessage(StreamPlayerPlaying);

		while (mPlayFlag == STATUS_PLAYING) {
			if (mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
				// ****从libmad处获取data******/
				int second = MP3Decoder.getAudioBuf(audioBuffer,
						mAudioMinBufSize);
				StreamPlayer.this.feed.updateTime(second);
				mAudioTrack.write(audioBuffer, 0, mAudioMinBufSize);

			} else {
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		mPlayFlag = STATUS_STOPED;
		handler.sendEmptyMessage(StreamPlayerStopped);
	}

	private int initAudioPlayer() {
		int samplerate = MP3Decoder.getAudioSamplerate();
		int channels = MP3Decoder.getAudioChannels();
		if (samplerate <= 0 || channels <= 0)
			return -1;

		samplerate = samplerate / channels;
		int audioChannel = channels == 1 ? AudioFormat.CHANNEL_OUT_MONO
				: AudioFormat.CHANNEL_OUT_STEREO;
		// 声音文件一秒钟buffer的大小
		mAudioMinBufSize = AudioTrack.getMinBufferSize(samplerate,
				audioChannel, AudioFormat.ENCODING_PCM_16BIT);
		Log.i(TAG, "buffer size is " + mAudioMinBufSize);

		mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, // 指定在流的类型
				// STREAM_ALARM：警告声
				// STREAM_MUSCI：音乐声，例如music等
				// STREAM_RING：铃声
				// STREAM_SYSTEM：系统声音
				// STREAM_VOCIE_CALL：电话声音
				samplerate,// 设置音频数据的采样率
				audioChannel,// 设置输出声道为双声道立体声
				AudioFormat.ENCODING_PCM_16BIT,// 设置音频数据块是8位还是16位
				mAudioMinBufSize, AudioTrack.MODE_STREAM);// 设置模式类型，在这里设置为流类型
		// AudioTrack中有MODE_STATIC和MODE_STREAM两种分类。
		// STREAM方式表示由用户通过write方式把数据一次一次得写到audiotrack中。
		// 这种方式的缺点就是JAVA层和Native层不断地交换数据，效率损失较大。
		// 而STATIC方式表示是一开始创建的时候，就把音频数据放到一个固定的buffer，然后直接传给audiotrack，
		// 后续就不用一次次得write了。AudioTrack会自己播放这个buffer中的数据。
		// 这种方法对于铃声等体积较小的文件比较合适。

		return 0;
	}

	static {
		System.loadLibrary("mad");
	}

	public class PlayerFeed {

		void exception(int code, String message) {
			Log.e(TAG, "exception from core: " + message);
			StreamPlayer.this.handler.post(new Runnable() {
				@Override
				public void run() {
					if (mPlayFlag == STATUS_PLAYING)
					stop();
				}
			});
			Message msg = new Message();
			msg.what = StreamPlayerError;
			msg.obj = message;
			StreamPlayer.this.handler.sendMessage(msg);
		}

		void updateTitle(String title) {
			Message msg = new Message();
			msg.what = StreamPlayerTitle;
			msg.obj = title;
			StreamPlayer.this.handler.sendMessage(msg);
		}

		void updateTime(int second){
			Log.i(TAG, "timer is: " + second);
			Message msg = new Message();
			msg.what = StreamPlayerTime;
			msg.arg1 = second;
			StreamPlayer.this.handler.sendMessage(msg);
		}
	}

}
