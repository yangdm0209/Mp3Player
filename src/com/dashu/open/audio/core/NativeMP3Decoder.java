package com.dashu.open.audio.core;

import com.dashu.open.audioplay.StreamPlayer;

public class NativeMP3Decoder {
	private int ret;

	public native int initAudioPlayer(String url, StreamPlayer.PlayerFeed feed);

	public native int getAudioBuf(short[] audioBuffer, int numSamples);

	public native void closeAduioPlayer();

	public native int getAudioSamplerate();
	public native int getAudioChannels();
	
	public static native int startPlay(String file, int StartAddr);
}
