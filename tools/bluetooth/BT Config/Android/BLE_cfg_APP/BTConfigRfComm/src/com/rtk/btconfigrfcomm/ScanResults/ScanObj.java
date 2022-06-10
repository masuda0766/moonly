package com.rtk.btconfigrfcomm.ScanResults;

import java.io.Serializable;

/** 
 ** class for devices list, including Bluetooth Device/ WiFi AP/ Extended AP
 **/
@SuppressWarnings("serial")
public  class ScanObj implements Serializable {
	private String mSsid;
	private String mMac;
	private int mRssi;
	private byte mEncrpytType;
	private byte mConnectStatus;
	private byte mConfigureStatus;
	
	// Bluetooth device 
	public ScanObj(String ssid, String mac, int rssi){
		this.mSsid 		= ssid;
		this.mMac 		= mac;
		this.mRssi		= rssi;
	}
	
	// WiFi AP
	public ScanObj(String ssid, String mac, int rssi, byte encrpytType){
		this.mSsid 	= ssid;
		this.mMac 	= mac;
		this.mRssi 	= rssi;
		this.mEncrpytType = encrpytType;
	}
	
	// Extended AP
	public ScanObj(String ssid, String mac, int rssi, byte encrpytType, 
			byte connectStatus, byte configureStatus){
		this.mSsid 	= ssid;
		this.mMac 	= mac;
		this.mRssi 	= rssi;
		this.mEncrpytType = encrpytType;
		this.mConnectStatus  = connectStatus;
		this.mConfigureStatus = configureStatus;
	}

	public String getSSID(){
		return mSsid;
	}
	
	public String getMac(){
		return mMac;
	}
	
	public int getRssi(){
		return mRssi;
	}
	
	public byte getEncrpytType(){
		return mEncrpytType;
	}
	
	public byte getConnectStatus(){
		return mConnectStatus;
	}
	
	public byte getConfigureStatus(){
		return mConfigureStatus;
	}
}

