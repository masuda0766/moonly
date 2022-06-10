package com.rtk.btconfigrfcomm.BTConfig;

import android.util.Log;

/**	The thread for connecting to repeater **/
public class BTConnectThread extends Thread{
	private String TAG = "BTConnectThread";
	
	private boolean mIsPause = false;
	
	/** interrupt the thread **/
	public synchronized void onThreadPause(){
		mIsPause = true;
		 try {
			this.wait();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}  
	}
	
	/** make the thread go-on **/
	public synchronized void onThreadResume() {  
        mIsPause = false;  
        this.notify();  
    }
	
	 /**  receive message from repeater **/
	@Override
	public void run() {
		while(!mIsPause && !isInterrupted()){
			Log.d(TAG,"connect to repeater");
			
			BTConfig.mBTConfig.setBTConfigState(BTConfigState.STATE_BT_CONNECTING);
			
			BTConfig.mBTConfig.getBTRfComm().cancelBTScan();
			String mBTDeviceMac = BTConfig.mBTConfig.getBTDeviceMac();
			
			boolean ret = BTConfig.mBTConfig.getBTRfComm().doBTRfCommConnect(mBTDeviceMac);
			if(ret == false){
				BTConfig.mBTConfig.setBTConfigState(BTConfigState.STATE_BT_CONNECT_FAIL);
				
			}else{
				BTConfig.mBTConfig.setBTConfigState(BTConfigState.STATE_BT_CONNECT_OK);
				
			} // connect successfully
			onThreadPause();
		}
	}
}
