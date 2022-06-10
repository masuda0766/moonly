package com.rtk.btconfig;

import java.util.Comparator;

import com.rtk.btconfigrfcomm.ScanResults.ScanObj;

public class ListSortByRssi implements Comparator<ScanObj>{

	@Override
	public int compare(ScanObj obj0, ScanObj obj1) {
		// TODO Auto-generated method stub
		int rssi0 = obj0.getRssi();
		int rssi1 = obj1.getRssi();
		if(rssi0 > rssi1){
			return -1;
		}else
			return 1;
	}
	
}
