package com.rtk.btconfig;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.InputType;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.rtk.Configuration.GlobalConfig;

import com.rtk.btconfig.UIMsgHandler;
import com.rtk.btconfigrfcomm.BTConfig.BTConfig;
import com.rtk.btconfigrfcomm.BTConfig.BTConfigState;
import com.rtk.btconfigrfcomm.ScanResults.ScanObj;

public class MainActivity extends Activity 
{

	final Context context = this;

	public static MainActivity mMainAct; 
	
	/*********** define ******************/
	protected static final String TAG = "MainActivity";
	
	/*choice for adjust position */
	public static int POSITION_CANCEL	  		=  0;
	public static int POSITION_ADJUSTMENT 		=  1;
	public static int POSITION_DEFAULT	  		= -1;
	public static int mChoiceForAdjust	  		= POSITION_DEFAULT;
	public static int mPositionIndex 	  		= -1; //0: too close, 1: fine, 2: too far
	private boolean isCheckPostiion 			= false;
	
	public static String SharePrefFile = "data";
	
	/*********** Main component ******************/
	/* target BT device */
	private static TextView	   	mText_device_name;
	/* target AP */
	private static TextView	   	mText_targetAP_SSID;
	private static TextView	   	mText_targetAP_MAC;
	private static ImageView   	mText_targetAP_securtiy;
	private static ImageView   	mText_targetAP_RSSI;
	/* Button */
	private static ImageButton  mImgBtn_version;
	private static ImageButton 	mImgBtn_searchDevice;
	private static ImageButton 	mImgBtn_siteSurvey;
	private static ImageButton 	mImgBtn_info_setting;
	
	/* ListView */
	private static ListView		mBTScanListView;
	private ListView mWlanScanListView_2G;
	private ListView mWlanScanListView_5G;
	
	/*custom dialog component*/
	private Button mConfirmButton;
	private Button mCancelButton;
	private EditText mHighRssiEditText;
	private EditText mLowRssiEditText;
	
	LinearLayout parentLayout_ap_list_view;
	LinearLayout layout_linear;
	
	private TextView mAPListTextView_2G;
	private TextView mAPListTextView_5G;
	
	/*********** Main Data ******************/
	public static SharedPreferences mSharePref;
	
	/* List */
	private List<ScanObj> mBTScanObjs = new ArrayList<ScanObj>();
	private List<ScanObj> mWlanScanObjs_2G = new ArrayList<ScanObj>();
	private List<ScanObj> mWlanScanObjs_5G = new ArrayList<ScanObj>();
	public  List<ScanObj> mExtendAPScanObjs = new ArrayList<ScanObj>();
	private List<HashMap<String, Object>>  mWlanArrayList_2G;
	private List<HashMap<String, Object>>  mWlanArrayList_5G;
	private List<HashMap<String, Object>>  mExtendAPArrayList;
	private List<HashMap<String, Object>>  mBTArrayList;
	
	/* BaseAdapter */
	private BaseAdapter mBTListAdapter;
	private BaseAdapter mWlanAdapter_2G; 
	private BaseAdapter mWlanAdapter_5G; 
	private BaseAdapter mExtendAPAdapter; 
	
	/* toast */
	public static Toast mToastBTNoResponse;
	public static Toast mToastBTWaitOnLine;
	public static Toast mToastBTDisconnected;
	
	/* ProgressDialog */
	private ProgressDialog pd = null;
	
	/* Dialog */
	private AlertDialog 		deviceList_alert = null;
	private AlertDialog.Builder deviceList_builder = null;
	
	/*********** variable ******************/
	/* remote AP profile */
	public byte[] mRemoteAPBuf;	
	public String mRemoteAPSSID;
	public String mRemoteAPMAC;
	
	private boolean mBTConnectedFlag 		    = false;/* connect to BT device */
	private boolean mAPConnectedFlag 		    = false;/* connect to remote AP */
	public static boolean mNeedShowAPConnectedDialog  = true; /* need show connect to AP OK dialog*/
	
	/*parse RSSI to position */
	public static com.rtk.UI.Rssi2Position Rssi2Position = new com.rtk.UI.Rssi2Position();
	
	public UIMsgHandler mUIHandler; 
	public BTConfigStateTimer mCheckBTConfigStateTimer;
	public static int mUIState = -1;
	
	public BTConfig mBTConfig; 
	public Handler mBTConfigHandler;
	public BTConfigThread mBTConfigThread;
	
	/* close socket active */
	public boolean mClickBackToBTUI 			= false;
	public boolean mActiveCloseSocket 			= false;
	
	/* previous connected BT */
	public static String mSavedBTName;   
	public static String mSavedBTMAC;

	public static Handler handler_pd;
	
	private Timer wait_timer = null;
	
	/*  set ListView height dynamically */
	private static ListViewHeight mListViewHeigth = new ListViewHeight();
	/* parse scanobjs to ListView on UI */
	private static ListFromScanObj mParseScanObj = new ListFromScanObj();
	
	public static boolean mWaitForBandResult 	= false; 	/* wait for band support result*/
	public static boolean mWaitForConnectACK 	= false; 	/* wait for ACK of connecting to AP*/
	public static boolean mIsDoingScanWlan   	= false; 	/* show scan wlan dialog */
	public static boolean mNeedShowWaitOnlineDialog = true; /* need show wait repeater onLine dialog*/
	public static boolean mIsDoingAdjustment 	= false; 	/* show position adjustment dialog */
	public static boolean mIsDoingConnectAP  	= false; /* show connect to AP dialog */
	
	public String targetAP_password = "";
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE); 
		setContentView(R.layout.activity_main);
		
		initData();
		initBTConfig();
		initComponent();
		initComponentAction();
		
		checkBTAdapterStatus();
	}

	protected void onStart()
	{
		super.onStart();
		
		final BluetoothAdapter mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();    
		if (mBluetoothAdapter.isEnabled()) {

		}else{
			AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
			builder.setTitle("Warning");
			builder.setMessage("Bluetooth Disabled!");
			builder.setPositiveButton("Turn on Bluetooth", new DialogInterface.OnClickListener(){

				@Override
				public void onClick(DialogInterface dialog, int which) {
					mBluetoothAdapter.enable(); 
					dialog.dismiss();
				}
				
			});
			builder.setNegativeButton("Cancel",new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog,int id) {
					dialog.cancel();
				}
			});
			builder.create().show();
		}
	}
	
	@Override
	protected void onPause() {
		// TODO Auto-generated method stub
		super.onPause();
		Log.d(TAG,"onPause");
	}
	
	@Override
	protected void onStop() 
	{
		super.onStop();
		Log.d(TAG,"onStop");
		mBTConnectedFlag = false;
		mAPConnectedFlag = false;
		resetTargetData();
		disconnectBT();
	}
	
	@Override
	protected void onDestroy() 
	{
		super.onDestroy();
		Log.d(TAG,"onDestroy");
		android.os.Process.killProcess(android.os.Process.myPid());
	}
	
	/** exit button **/
    @Override
	public void onBackPressed() {
    	AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
		builder.setTitle("Warning");
		builder.setMessage("Do you exit BT Config?");
		builder.setPositiveButton("Yes", new DialogInterface.OnClickListener(){

			@Override
			public void onClick(DialogInterface dialog, int which) {
				onDestroy();
			}
			
		});
		builder.setNegativeButton("No",new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog,int id) {
				dialog.cancel();
			}
		});
		builder.setCancelable(false);
		builder.create().show();
	}
	
	private void initData() {
		
		mMainAct = this;
		mUIHandler = new UIMsgHandler(); 
		
		mSharePref = getSharedPreferences(SharePrefFile, 0);
		
		mBTConnectedFlag = false;
		mAPConnectedFlag = false;
	}

	private void initBTConfig()
	{
		if(mBTConfig == null)
			mBTConfig = new BTConfig(mUIHandler);
		
		if(mBTConfigThread == null){
			mBTConfigThread = new BTConfigThread("BTConfig");
			mBTConfigThread.hBTConfig = mBTConfig;
			mBTConfigThread.start();
        }
		
		if(mBTConfigHandler == null)
        	mBTConfigHandler = mBTConfigThread.getHandler();
		
		if(mCheckBTConfigStateTimer == null){
        	mCheckBTConfigStateTimer = new BTConfigStateTimer();
            mCheckBTConfigStateTimer.startCheckStatusTimer(mBTConfig);
        }
	}
	
	private void initComponent() 
	{
		View wlanScanLayout = View.inflate(mMainAct, R.layout.wlan_scan_listview, null);
		
		mText_device_name 		= (TextView)	findViewById(R.id.textDeviceName);
		
		mText_targetAP_SSID		= (TextView)	findViewById(R.id.textSSID_target);
		mText_targetAP_MAC		= (TextView)	findViewById(R.id.textMAC_target);
		mText_targetAP_securtiy	= (ImageView)	findViewById(R.id.img_target_encrypt);
		mText_targetAP_RSSI		= (ImageView)	findViewById(R.id.img_target_rssi);
		
		mImgBtn_version			= (ImageButton) findViewById(R.id.btn_version);
		mImgBtn_searchDevice 	= (ImageButton) findViewById(R.id.btn_gosetting);
		mImgBtn_siteSurvey 		= (ImageButton) findViewById(R.id.btn_site_survey);
		mImgBtn_info_setting 	= (ImageButton) findViewById(R.id.btn_info_setting);
		
		layout_linear = new LinearLayout(mMainAct);
		parentLayout_ap_list_view = (LinearLayout) findViewById(R.id.layer1_ap_list_view);
		layout_linear.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

		mWlanScanListView_2G = (ListView) wlanScanLayout.findViewById(R.id.wlanScanListID_2G);	//2G list
		mWlanScanListView_5G = (ListView) wlanScanLayout.findViewById(R.id.wlanScanListID_5G);	//5G list
		parentLayout_ap_list_view.addView(wlanScanLayout, layout_linear.getLayoutParams());
		
		mWlanScanListView_2G 	= (ListView) wlanScanLayout.findViewById(R.id.wlanScanListID_2G);	//2G list
		mWlanScanListView_5G 	= (ListView) wlanScanLayout.findViewById(R.id.wlanScanListID_5G);	//5G list
		
		mAPListTextView_2G 		= (TextView) wlanScanLayout.findViewById(R.id.wlanAPTVID_2G);
		mAPListTextView_5G 		= (TextView) wlanScanLayout.findViewById(R.id.wlanAPTVID_5G);
	}

	private void initComponentAction() 
	{
		
		mImgBtn_version.setOnClickListener(new OnClickListener(){
			@Override
			public void onClick(View v) {
				checkVersion_action();
			}
		});
		
		mImgBtn_searchDevice.setOnClickListener(new OnClickListener(){
			@Override
			public void onClick(View v) {
				Log.d(TAG,"Search BT Device.");
				searchBTDevice_action();
			}
		});
		
		mImgBtn_siteSurvey.setOnClickListener(new OnClickListener(){
			@Override
			public void onClick(View v) {
				Log.d(TAG,"Site Survey Start");
				siteSurvey_action();
			}
		});
		
		mImgBtn_info_setting.setOnClickListener(new OnClickListener(){
			@Override
			public void onClick(View v) {
				Log.d(TAG,"Info Setting");
				info_setting_action();
			}
		});
		
		handler_pd = new Handler() {
			@Override
			public void handleMessage(Message msg) {
				// Log.d(TAG,"handleMessage msg.what: " + String.valueOf(msg.what));
				switch (msg.what) {
				case 0:
					if(pd!=null){
						if(pd.isShowing())
							pd.dismiss();
					}
					
					break;
				default:
					break;
				}
			}
		};
	}
	
	public void checkVersion_action()
	{
		String appVersion = "";

		PackageManager manager = this.getPackageManager();
		PackageInfo info = null;
		try {
			info = manager.getPackageInfo(this.getPackageName(), 0);
		} catch (NameNotFoundException e) {
			e.printStackTrace();
		}
		appVersion = info.versionName;

		if (appVersion.length() == 0)
			appVersion = "1.0.0";
		
		AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
		builder.setTitle("About BTConfig");
		builder.setMessage("Version : "+appVersion);
		builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

			@Override
			public void onClick(DialogInterface dialog, int which) {
				dialog.dismiss();
			}
			
		});
		builder.create().show();
	}
	
	/** check Bluetooth enable status */
    public void checkBTAdapterStatus(){
    	int btAdapterStatus = mBTConfig.getBTRfComm().getBTAdapterStatus();
    	if(btAdapterStatus == 1){
    		//Cmd4BTConfigThread.sendCommonCmd(BTConfigThread.CMD_START_SCAN_BT, mBTConfigHandler);
    		searchBTDevice_action();
    	}else if(btAdapterStatus == 0){
    		//mDialogTurnOnBT.show();
    		
    		Log.e(TAG,"mDialogTurnOnBT.show();");
    		
    	}else if(btAdapterStatus == -1){
    		ToastOps.getToast("Sorry, your device does not support Bluetooth").show();
    		return;
    	}
    }
	
	private void searchBTDevice_action() 
	{
		mBTConnectedFlag = false;
		mAPConnectedFlag = false;
		resetTargetData();
		
		Log.d(TAG,"disconnectBT");
		
		disconnectBT();
		
		Log.d(TAG,"scan_BT_peripheral");
		
		scan_BT_peripheral();

	}

	private void resetTargetData() {
		mText_device_name.setText("BT Device");
		mText_device_name.setTextColor(Color.parseColor("#000000"));
		mText_targetAP_SSID.setText("");
		mText_targetAP_MAC.setText("");
		mText_targetAP_securtiy.setImageResource(0);
		mText_targetAP_RSSI.setImageResource(0);
		
		mWlanScanObjs_2G.clear();
		mWlanArrayList_2G = mParseScanObj.getArrayList(mWlanScanObjs_2G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_2G = new ListBaseAdapter(mMainAct, mWlanArrayList_2G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_2G.setAdapter(mWlanAdapter_2G);
		mWlanScanListView_2G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_2G);
		mWlanScanObjs_5G.clear();
		mWlanArrayList_5G = mParseScanObj.getArrayList(mWlanScanObjs_5G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_5G = new ListBaseAdapter(mMainAct, mWlanArrayList_5G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_5G.setAdapter(mWlanAdapter_5G);
		mWlanScanListView_5G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_5G);
		
		mAPListTextView_2G.setVisibility(View.INVISIBLE);
		mAPListTextView_5G.setVisibility(View.INVISIBLE);
		
		restParams();
	}

	/** reset parameters to original **/
   	public void restParams(){
   		
   		
   		if(mCheckBTConfigStateTimer != null){
			mCheckBTConfigStateTimer.mRepeaterRedetect = false;
   		}
   		
		mActiveCloseSocket = true;
		
   		//mRemoteAPSSID 		= null;
		mChoiceForAdjust 	= POSITION_DEFAULT;
		
		mIsDoingAdjustment 	= false;
		mIsDoingConnectAP  	= false;
		mIsDoingScanWlan   	= false;
		//mIsDoingSetting	   	= false;
		//mIsDoingExit       	= false;
		
		mNeedShowAPConnectedDialog		= true;
		
		mWaitForConnectACK  = false;
		mWaitForBandResult  = false;
		
		//mNeedShowFoundPreBTDialog = false;
		
		targetAP_password = "";
		
   	}
	
	private void siteSurvey_action() 
	{
		if(mBTConnectedFlag==false){
			AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
			builder.setTitle("Warning");
			builder.setCancelable(false);
			builder.setMessage("Please scan BT device to connect it.");
			builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
				
			});
			builder.create().show();
			return;
		}
		
		startWaiting("Scanning WiFi AP","waiting ...",30,"Scan Timeout");
		
		//if(mIsDoingAdjustment)
		//	return;
		if(mCheckBTConfigStateTimer.mIsNoResponse)
		{
			mToastBTNoResponse.show();
			return;
		}else if(mBTConfig.getBTConfigState() == BTConfigState.STATE_BT_REPEATER_OFFLINE
				|| mCheckBTConfigStateTimer.mRepeaterRedetect){
			
			if(GlobalConfig.CONFIG_OFFLINE_DECTECTION)
				mToastBTWaitOnLine.show();
			else
				mToastBTDisconnected.show();
			
			return;
		}
		mIsDoingScanWlan = true;
		
		mWlanScanObjs_2G.clear();
		mWlanArrayList_2G = mParseScanObj.getArrayList(mWlanScanObjs_2G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_2G = new ListBaseAdapter(mMainAct, mWlanArrayList_2G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_2G.setAdapter(mWlanAdapter_2G);
		mWlanScanListView_2G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_2G);
		mWlanScanObjs_5G.clear();
		mWlanArrayList_5G = mParseScanObj.getArrayList(mWlanScanObjs_5G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_5G = new ListBaseAdapter(mMainAct, mWlanArrayList_5G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_5G.setAdapter(mWlanAdapter_5G);
		mWlanScanListView_5G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_5G);
		
		
		queryWlanBand();
	}
	
	private void info_setting_action() 
	{
		
		// custom dialog
		final Dialog custom_dialog = new Dialog(context);
		
		final int highRssi = mSharePref.getInt(GlobalConfig.KEY_HIGH_RSSI, GlobalConfig.DEFAULT_HIGH_RSSI);
   		final int lowRssi  = mSharePref.getInt(GlobalConfig.KEY_LOW_RSSI, GlobalConfig.DEFAULT_LOW_RSSI);
		
		int rssi_dbm_high = highRssi -100;
		int rssi_dbm_low  = lowRssi - 100;
		
		custom_dialog.setContentView(R.layout.alert_dialog);
		custom_dialog.setTitle("Configurations");
		custom_dialog.setCancelable(false);
		
		// set the custom dialog components - text and button
        mHighRssiEditText 	= (EditText)custom_dialog.findViewById(R.id.high_rssi_input);
        mLowRssiEditText 	= (EditText)custom_dialog.findViewById(R.id.low_rssi_input);
        mConfirmButton 		= (Button)custom_dialog.findViewById(R.id.confirm_button);
        mCancelButton 		= (Button)custom_dialog.findViewById(R.id.cancel_button);
        
        mHighRssiEditText.setText( String.valueOf(rssi_dbm_high));
        mLowRssiEditText.setText( String.valueOf(rssi_dbm_low) );
        
        mConfirmButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
            	
            	int h = Integer.valueOf(mHighRssiEditText.getText().toString());
            	int l = Integer.valueOf(mLowRssiEditText.getText().toString());
            	//Toast.makeText(	MainActivity.this,"dBm: "+h+" ~ "+l,Toast.LENGTH_LONG).show();
            	
            	if(h>l && h<0 && l<0 && h>=-100 && l>=-100){
            		
            		int h_rssi_input = h + 100;
            		int l_rssi_input = l  + 100;

                	
                	SharedPreferences.Editor editor = mSharePref.edit();  
           		 	editor.putInt(GlobalConfig.KEY_HIGH_RSSI, h_rssi_input);
           		 	editor.putInt(GlobalConfig.KEY_LOW_RSSI, l_rssi_input);
           		 	editor.commit();
                	
                	//Toast.makeText(	MainActivity.this,"RSSI: "+g_rssi_high+" ~ "+g_rssi_low,Toast.LENGTH_LONG).show();
            	}else{
            		Toast.makeText(	MainActivity.this,"invaild RSSI value",Toast.LENGTH_LONG).show();
            	}

            	custom_dialog.dismiss();
            }
        });
        mCancelButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
            	custom_dialog.dismiss();
            }
        });
        
		custom_dialog.show();
	}

	private void scan_BT_peripheral() 
	{

    	mBTScanListView = new ListView(mMainAct);
    	
    	mBTScanListView.setFocusable(false);
    	mBTScanListView.setFocusableInTouchMode(false);
    	
    	deviceList_builder=new AlertDialog.Builder(mMainAct);
		deviceList_builder.setTitle("Choose BT device to configure it.");
		deviceList_builder.setCancelable(false);
		
		mBTArrayList = mParseScanObj.getArrayList(mBTScanObjs,ListBaseAdapter.LIST_SCAN_BT);
		mBTListAdapter = new ListBaseAdapter(mMainAct, mBTArrayList,ListBaseAdapter.LIST_SCAN_BT);
		mBTScanListView.setAdapter(mBTListAdapter);
		
		deviceList_builder.setPositiveButton("Cancel", null);
    	
		deviceList_builder.setView(mBTScanListView);
    	deviceList_alert = deviceList_builder.create();
    	deviceList_alert.show();
		
    	startWaiting("Scan BT Device","Waiting...",30,"Scan Timeout");
    	
		updateBTScanUI(null);
		Cmd4BTConfigThread.sendCommonCmd(BTConfigThread.CMD_START_SCAN_BT, mBTConfigHandler);
	}

	private void disconnectBT() 
	{
		//mBTConfig.setStopBTRecv();
		mBTConfig.closeBTSocket();
		mActiveCloseSocket = true;
	}
	
	public void savePreBT(){
		SharedPreferences.Editor editor = mSharePref.edit();  
		editor.putString(GlobalConfig.KEY_PREVIOUSE_BT_NAME, mSavedBTName);
	   	editor.putString(GlobalConfig.KEY_PREVIOUSE_BT_MAC, mSavedBTMAC);
	   	editor.commit(); 
	}
	
	public void updateBTScanUI(List<ScanObj> btScanList){
		mBTScanObjs.clear();
		mBTListAdapter.notifyDataSetChanged();
		
		if(btScanList != null && mBTScanListView!=null){
			//Log.d(TAG,"!!! stopWaiting !!! updateBTScanUI");
			stopWaiting();
			
			for(ScanObj mScanObj: btScanList){
				//filter Bluetooth by name
				if(GlobalConfig.CONFIG_FILTER_BT){
					String filterName = mSharePref.getString(GlobalConfig.KEY_FILTER_BT_NAME,GlobalConfig.DEFAULT_FILTER_BT_NAME);
					if(mScanObj.getSSID().contains(filterName)){
						mBTScanObjs.add(mScanObj);	
					}
					
				}else{
					mBTScanObjs.add(mScanObj);	
				}
				
			}// for
			sortBTByRSSI();
			detectPreBT();
			
			Log.d(TAG,"sortBTByRSSI====="+mBTScanObjs.size());
			
			mBTArrayList = mParseScanObj.getArrayList(mBTScanObjs,ListBaseAdapter.LIST_SCAN_BT);
			mBTListAdapter = new ListBaseAdapter(mMainAct, mBTArrayList,ListBaseAdapter.LIST_SCAN_BT);
			mBTListAdapter.notifyDataSetChanged();
			mBTScanListView.setAdapter(mBTListAdapter);
			mListViewHeigth.setListViewHeightBasedOnChildren(mBTScanListView);
			mBTScanListView.setOnItemClickListener(new AdapterView.OnItemClickListener(){
				@Override
	            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
					
					if(deviceList_alert!=null){
						deviceList_alert.cancel();
						deviceList_alert = null;
					}
					
	                //Toast.makeText(getApplicationContext(), "You choose " + mBTScanObjs.get(position).getSSID(), Toast.LENGTH_SHORT).show();

		           	mSavedBTName = mBTScanObjs.get(position).getSSID();
	            	mSavedBTMAC = mBTScanObjs.get(position).getMac();
	            	
	            	mText_device_name.setText(mSavedBTName);
	            	mText_device_name.setTextColor(Color.parseColor("#0000FF"));
	            	
	            	Cmd4BTConfigThread.sendConnectBTCmd(mSavedBTMAC, mBTConfigHandler);
	            	
	            	startWaiting("Connecting to "+mSavedBTName,"waiting...",30,"Connection Timeout");
	            }
			});
		
		}else{
			mBTScanListView.setAdapter(null);
			mListViewHeigth.setListViewHeightBasedOnChildren(mBTScanListView);
			mBTScanListView.setOnItemClickListener(null);
		}

	}
	
	/** sort Bluetooth by RSSI **/
	private void sortBTByRSSI( ){
		if(mBTScanObjs.isEmpty())
			return;
		
		Collections.sort(mBTScanObjs, new ListSortByRssi()); // sort by rssi
	}
	
	private void detectPreBT(){
		if(mBTScanObjs.isEmpty())
			return;
		
		//saved previous Bluetooth profile
		if(GlobalConfig.CONFIG_SAVE_BT_PROFILE){
			
			String preBTName = mSharePref.getString(GlobalConfig.KEY_PREVIOUSE_BT_NAME, GlobalConfig.DEFAULT_PREVIOUSE_BT_NAME);
			String preBTMac = mSharePref.getString(GlobalConfig.KEY_PREVIOUSE_BT_MAC, GlobalConfig.DEFAULT_PREVIOUSE_BT_MAC);
			
			//for(ScanObj mScanObj: mBTScanObjs){
			//	Log.e(TAG, "mScanObj: "+ mScanObj.getSSID() + ", "+ mScanObj.getMac());
			//	Log.w(TAG, "preBT: "+ preBTName + ", "+ preBTMac);
			//}
		}//GlobalConfig.CONFIG_SAVE_BT_PROFILE
	}
	
	public void BTconnectOK()
	{
		mBTConnectedFlag = true;
	}
	
	public void showBTdisconnection()
	{
		if(mBTConnectedFlag){
			mBTConnectedFlag = false;
			AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
			builder.setTitle("Warning");
			builder.setMessage("Please check if BT device is exist.");
			builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
				
			});
			builder.create().show();
			
			mBTConnectedFlag = false;
			mAPConnectedFlag = false;
			resetTargetData();
			disconnectBT();
		}
			
	}
	
	/** query wlan band capability * **/
	 public void queryWlanBand(){
		 setUIToWlanScanUI(); // refresh the scan results on UI
		 //mDialogScanWlanAP.show();
		 mWaitForBandResult = true;
		 
		 Cmd4BTConfigThread.sendCommonCmd(BTConfigThread.CMD_QUERY_WLAN_BAND, 
		 		 mBTConfigHandler);
	 }
	 
	 /** init wlan scan UI  * ***/
    private void setUIToWlanScanUI()
    {
	
		mWlanScanListView_2G.setOnItemClickListener(new AdapterView.OnItemClickListener(){

			@Override
			public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
				//if(mIsDoingAdjustment)
				//	return;
				if(mCheckBTConfigStateTimer.mIsNoResponse)
				{
					mToastBTNoResponse.show();
					return;
				}else if(mBTConfig.getBTConfigState() == BTConfigState.STATE_BT_REPEATER_OFFLINE
						|| mCheckBTConfigStateTimer.mRepeaterRedetect){
					
					if(GlobalConfig.CONFIG_OFFLINE_DECTECTION)
						mToastBTWaitOnLine.show();
					else
						mToastBTDisconnected.show();
					
					return;
				}
				mUIState = UIState.STATE_UI_SHOW_CHOICE_FOR_AP;
				mIsDoingConnectAP = true;
				
				showAPConnectDialog((byte)0, position);
			}
			
		});
    	mWlanScanListView_5G.setOnItemClickListener(new AdapterView.OnItemClickListener(){

			@Override
			public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
				//if(mIsDoingAdjustment)
				//	return;
				if(mCheckBTConfigStateTimer.mIsNoResponse)
				{
					mToastBTNoResponse.show();
					return;
				}else if(mBTConfig.getBTConfigState() == BTConfigState.STATE_BT_REPEATER_OFFLINE
						|| mCheckBTConfigStateTimer.mRepeaterRedetect){
					
					if(GlobalConfig.CONFIG_OFFLINE_DECTECTION)
						mToastBTWaitOnLine.show();
					else
						mToastBTDisconnected.show();
					
					return;
				}
				mUIState = UIState.STATE_UI_SHOW_CHOICE_FOR_AP;
				mIsDoingConnectAP = true;
				
				showAPConnectDialog((byte)1, position);
			}
    		
    	});
    }
	 
	 protected void updateWlan2GScanUI(){
		mWlanScanObjs_2G.clear();

		List<ScanObj> tmpScanResults = mBTConfig.getWlanScanResults(0);
		Log.i(TAG,"2G tmpScanResults.size() : " + tmpScanResults.size());
		
		for(int i = 0; i < tmpScanResults.size(); i++){
			mWlanScanObjs_2G.add(tmpScanResults.get(i));
		}
		
		mWlanArrayList_2G = mParseScanObj.getArrayList(mWlanScanObjs_2G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_2G = new ListBaseAdapter(mMainAct, mWlanArrayList_2G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_2G.setAdapter(mWlanAdapter_2G);
		mWlanScanListView_2G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_2G);
		
		if(!mWlanScanObjs_2G.isEmpty())
			mAPListTextView_2G.setVisibility(View.VISIBLE);
		
		if(mBTConfig.getWlanBand_5G() != 1){ // not support 5G 
			mUIState = UIState.STATE_UI_DISMISS_SCAN_WLAN;
			mIsDoingScanWlan = false;
		}
    }
	    
    protected void updateWlan5GScanUI(){
    	mWlanScanObjs_5G.clear();

    	List<ScanObj> tmpScanResults = mBTConfig.getWlanScanResults(1);
		Log.i(TAG,"5G tmpScanResults.size() : " + tmpScanResults.size());
		
		for(int i = 0; i < tmpScanResults.size(); i++){
			mWlanScanObjs_5G.add(tmpScanResults.get(i));
		}
		
		mWlanArrayList_5G = mParseScanObj.getArrayList(mWlanScanObjs_5G,ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanAdapter_5G = new ListBaseAdapter(mMainAct, mWlanArrayList_5G, ListBaseAdapter.LIST_SCAN_WLAN_AP);
		mWlanScanListView_5G.setAdapter(mWlanAdapter_5G);
		mWlanScanListView_5G.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mWlanScanListView_5G);
		
		if(!mWlanScanObjs_5G.isEmpty())
			mAPListTextView_5G.setVisibility(View.VISIBLE);
		
		mUIState = UIState.STATE_UI_DISMISS_SCAN_WLAN;
		mIsDoingScanWlan   = false;
    }
    
    /***  update extended AP Info UI and judge the repeater connection status * ***/
	public  void updateExtendedAPInfoUI()
	{
		if(mWaitForConnectACK || mIsDoingScanWlan)
			return;
		
		mExtendAPScanObjs.clear();
		
		List<ScanObj> tmpExtendedAPs = mBTConfig.getExtendAPObjs();
		
		for(int i = 0; i < tmpExtendedAPs.size(); i++){
			if(tmpExtendedAPs.get(i) != null){
				if(tmpExtendedAPs.get(i).getConfigureStatus() == 0){ // never configure the repeater before
					return;
				}
				mExtendAPScanObjs.add( tmpExtendedAPs.get(i));
			}
		}
		
		byte tmpConnectStatus = mExtendAPScanObjs.get(0).getConnectStatus();
		String target_SSID 		= (String)mExtendAPScanObjs.get(0).getSSID();
        String target_MAC 		= (String)mExtendAPScanObjs.get(0).getMac();
        int target_securtiy 	=  (int)mExtendAPScanObjs.get(0).getEncrpytType();
        int target_rssi 		= (int)mExtendAPScanObjs.get(0).getRssi();
		
		mText_targetAP_SSID.setText(target_SSID);
		mText_targetAP_MAC.setText(target_MAC);
		if(target_securtiy==0){
			mText_targetAP_securtiy.setImageResource(R.drawable.encrypt_open);
		}else{
			mText_targetAP_securtiy.setImageResource(R.drawable.encrypt_lock);
		}
		Log.i(TAG,"Ap connection info("+tmpConnectStatus+"): "+target_SSID+" security:"+target_securtiy+" rssi:"+target_rssi);
		
		judgeConnectStatus(target_SSID);
		
		/*
		mExtendAPArrayList = mParseScanObj.getArrayList(mExtendAPScanObjs,ListBaseAdapter.LIST_EXTENDED_AP);
		mExtendAPAdapter = new ListBaseAdapter(mMainAct, mExtendAPArrayList, ListBaseAdapter.LIST_EXTENDED_AP);
		mExtendAPListView.setAdapter(mExtendAPAdapter);
		mExtendAPListView.setVisibility(View.VISIBLE);
		mListViewHeigth.setListViewHeightBasedOnChildren(mExtendAPListView);
		
		if(!mExtendAPScanObjs.isEmpty())
			mExtendedTextView.setVisibility(View.VISIBLE);
		else
			return;
		
		String ssid = mExtendAPScanObjs.get(0).getSSID();
		judgeConnectStatus(ssid);*/
	}
    
	/** judge the repeater connection status * ***/
	private void judgeConnectStatus(String ssid){
		byte tmpConnectStatus = mExtendAPScanObjs.get(0).getConnectStatus();

		if(tmpConnectStatus == 0x04){ /* connect successfully */
			connectToAPOK();
		}
		else if(tmpConnectStatus == 0x0f){	/* wrong password */
			//if(!GlobalConfig.CONFIG_POSITION_DECTECTION)
			//	mDialogAPConnected.dismiss();
			mText_targetAP_RSSI.setImageResource(R.drawable.strength_disconnection);
			wrongKeyForAP(ssid);
		}
		else{ /* connecting or connect fail */
			//if(!GlobalConfig.CONFIG_POSITION_DECTECTION)
			//	mDialogAPConnected.dismiss();
			mText_targetAP_RSSI.setImageResource(R.drawable.strength_disconnection);
			connectToAPFail();
		}
	}
	
	/***  update extended AP Info UI and judge the repeater connection status * ***/
	public  void setAPDisconnectOnUI(String ssid, byte encrypt){
		if(mWaitForConnectACK)
			return;
		
		mExtendAPScanObjs.clear();
		
		ScanObj remoteAP = new ScanObj(ssid, "", (byte)0, encrypt, (byte)0, (byte)1);
		mExtendAPScanObjs.add( remoteAP );
		
		mExtendAPArrayList = mParseScanObj.getArrayList(mExtendAPScanObjs,ListBaseAdapter.LIST_EXTENDED_AP);
		mExtendAPAdapter = new ListBaseAdapter(mMainAct, mExtendAPArrayList, ListBaseAdapter.LIST_EXTENDED_AP);
		//mExtendAPListView.setAdapter(mExtendAPAdapter);
		//mExtendAPListView.setVisibility(View.VISIBLE);
		//mListViewHeigth.setListViewHeightBasedOnChildren(mExtendAPListView);
		
		//mExtendedTextView.setVisibility(View.VISIBLE);
		
	 }
	
	//TODO
    public void showAPConnectDialog(byte band, int index){
		List<ScanObj> apScanObjs;
		if(band == 0){
			apScanObjs = mWlanScanObjs_2G;
		}else if(band == 1){
			apScanObjs = mWlanScanObjs_5G;
		}else 
			return;
		
		final byte sendBand = band;
		final byte encryptType = apScanObjs.get(index).getEncrpytType();
		final String sendSSID = apScanObjs.get(index).getSSID();
		final String sendMAC = apScanObjs.get(index).getMac();
		
		
		AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);
		final EditText edittext = new EditText(context);
		
		//check security type
		if(encryptType == 1 || encryptType==2){// 1:WPA/WPA2 Key  2:WEP Key
			String savedPwd = "";
			if(GlobalConfig.CONFIG_SAVE_AP_PWD){
				savedPwd = mSharePref.getString(sendSSID+""+sendMAC, "");
			}
			edittext.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
			edittext.setText(savedPwd);
			alertDialogBuilder.setTitle("Enter the AP passowrd:");
			alertDialogBuilder.setView(edittext);
		}else if(encryptType ==  0){//open
			alertDialogBuilder.setTitle("The target AP is no security.");
		}
		
		alertDialogBuilder.setCancelable(false);
		alertDialogBuilder.setPositiveButton("Next",new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog,int id) {
				
				boolean isAPProfileRight = false;
           	 	String passwordStr = null;
           	 	byte sendEncrpyt = 1;
				
				if (encryptType == 0) {	/* AP is open */
					sendEncrpyt = 0;
					isAPProfileRight = true;
				}else{					/* AP is not open */
					targetAP_password = edittext.getText().toString();
					passwordStr = targetAP_password;
					// Check the legality of the input password
					int checkPwdFlag = APPasswordCheck.checkWifiPassWord(passwordStr.getBytes(),encryptType); 

					if (checkPwdFlag == 1) {/* password in right format */
						isAPProfileRight = true;
					}
					
					if(checkPwdFlag == 1){/* password in right format */
               			isAPProfileRight = true;
               	 	}else{/* password in wrong format */
               	 		isAPProfileRight = false;

		            	if(checkPwdFlag  ==  0 && encryptType == 1){
		            		Log.e(TAG,"WPA_WRONG_PASSWORD_FORMAT");
		            		//ToastOps.getToast( "Invalid WPA/WPA2 Key! Key Length Must >=8 characters").show();
		            	}else if(checkPwdFlag == 0 && encryptType == 2){
		            		Log.e(TAG,"WEP_WRONG_PASSWORD_FORMAT");
		            		//ToastOps.getToast( "Invalid WEP Key! Key Length Must Be 5,10,13,26 characters").show();
		            	}else{
		            		Log.e(TAG,"Invalid Key");
		            		//ToastOps.getToast( "Invalid Key!").show();
		            	}
		            	
		            	AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
		        		builder.setTitle("Warning");
		        		builder.setMessage("The password is invalid!");
		        		builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

		        			@Override
		        			public void onClick(DialogInterface dialog, int which) {
		        				dialog.dismiss();
		        			}
		        			
		        		});
		        		builder.create().show();
		            	
               	 	}// wrong password format
				} 
				
				if(isAPProfileRight){

	           		 mIsDoingConnectAP = false;
	           		 mAPConnectedFlag = false;
	           		 mChoiceForAdjust = POSITION_DEFAULT;
	           		 
	           		 mUIState = UIState.STATE_UI_SHOW_CONNECTING_AP;
	           		 
	           		 isCheckPostiion = true;
	           		 setAPDisconnectOnUI(sendSSID, sendEncrpyt);
	           		 startWaiting("Connecting to "+sendSSID,"waiting...",30,"Connection Timeout");
	           		 
	           		 mRemoteAPSSID = sendSSID;
	           		 mRemoteAPMAC = sendMAC;
	           		 
	           		 if(GlobalConfig.CONFIG_SAVE_AP_PWD && encryptType != 0){
	           			 SharedPreferences.Editor editor = mSharePref.edit();
	           			 editor.putString(mRemoteAPSSID+""+mRemoteAPMAC, passwordStr);
	           			 editor.commit();
	           		 }
	           		 mNeedShowAPConnectedDialog = true;
	           		 mWaitForConnectACK = true;
	           		 
	           		 Log.w(TAG, "send profile: "+ sendSSID.toString()+"/"+passwordStr);
	           		 
	           		 mRemoteAPBuf = constructAPProfile(sendBand, sendEncrpyt, sendSSID, sendMAC, passwordStr);
	           		 Cmd4BTConfigThread.sendAPProfileCmd(mRemoteAPBuf, mBTConfigHandler);
	           	 }
			}
		});
		alertDialogBuilder.setNegativeButton("Cancel",new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog,int id) {
				dialog.cancel();
			}
		});

		AlertDialog alertDialog = alertDialogBuilder.create();
		alertDialog.show();

	}//showAPConnectDialog
    
    /** wifi's client connects to AP successfully, and show position adjustment dialog **/
    //TODO
	private void connectToAPOK()
	{
		//Log.d(TAG,"!!! stopWaiting !!! connectToAPOK");
		stopWaiting();
		mAPConnectedFlag = true;
		
		int currentRssi =  mExtendAPScanObjs.get(0).getRssi();
		
		if(GlobalConfig.CONFIG_POSITION_DECTECTION){
			/* parse rssi to position index */
			
			int tmpPositionIndex = -1; //Rssi2Position.getPostionRangeIndex(tmpRSSI);
			
			int highRssi = MainActivity.mSharePref.getInt(GlobalConfig.KEY_HIGH_RSSI, GlobalConfig.DEFAULT_HIGH_RSSI);
			int lowRssi  = MainActivity.mSharePref.getInt(GlobalConfig.KEY_LOW_RSSI, GlobalConfig.DEFAULT_LOW_RSSI);
			
			tmpPositionIndex = Rssi2Position.getRssiRangeIndex((byte)(currentRssi+100),highRssi,lowRssi);
			Log.d(TAG, "showPositionAdjustDialog--- "+tmpPositionIndex);
			
			/* show position adjustment dialog */
			showPositionAdjustDialog(tmpPositionIndex);
	        
			//int current_target_rssi = currentRssi+100;
	        byte strength = mParseScanObj.parseWifiStrength(currentRssi);
	        mText_targetAP_RSSI.setImageResource(mParseScanObj.getStrengthDrawable(strength));
	        
		}else{
			if(!mNeedShowAPConnectedDialog)
				return;
			
			mNeedShowAPConnectedDialog = false;
			
			String target_SSID 		= (String)mExtendAPScanObjs.get(0).getSSID();
	        String target_MAC 		= (String)mExtendAPScanObjs.get(0).getMac();
	        int target_securtiy 	=  (int)mExtendAPScanObjs.get(0).getEncrpytType();
        
			mText_targetAP_SSID.setText(target_SSID);
			mText_targetAP_MAC.setText(target_MAC);
			if(target_securtiy==0){
				mText_targetAP_securtiy.setImageResource(R.drawable.encrypt_open);
			}else{
				mText_targetAP_securtiy.setImageResource(R.drawable.encrypt_lock);
			}
			
			Log.e(TAG,"Ap Connected: "+target_SSID+" security:"+target_securtiy+" rssi:"+currentRssi);
			
		}
	}
    
	/** show repeater position adjustment dialog
	 * 1. too far/too close, default choose adjust position
	 * 2. fine, default no longer adjust position
	 * **/
	protected void showPositionAdjustDialog(final int positionIndex){
		if(mClickBackToBTUI) // click back button
			return;
		if(positionIndex == -1 ) //0, 1, 2 is right value
			return;
		if(mChoiceForAdjust == POSITION_CANCEL) // cancel adjust
			return;
		if(mIsDoingConnectAP || mIsDoingScanWlan) // show other dialog
			return;
		
		mNeedShowWaitOnlineDialog = false;
		
		mUIState = UIState.STATE_UI_SHOW_CHOICE_FOR_ADJUSTMENT;
		
		mPositionIndex = positionIndex;
		
		Log.d(TAG,"positionIndex: "+positionIndex);
		
		if(isCheckPostiion){
			isCheckPostiion = false;
			
			if(positionIndex==0){
				Log.d(TAG,"POSITION TOO CLOSE");
				mIsDoingAdjustment = false;
			}else if(positionIndex==1){
				Log.d(TAG,"POSITION FINE");
				mIsDoingAdjustment = false;
			}else if(positionIndex==2){
				Log.e(TAG,"POSITION TOO FAR");
				
				mIsDoingAdjustment = true;
				
				AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
				builder.setTitle(mSavedBTName+"'s Position Too Far From AP!");
				builder.setMessage("Suggest to adjust position close to AP!");
				builder.setPositiveButton("NO,thanks!",new DialogInterface.OnClickListener(){

					@Override
					public void onClick(DialogInterface dialog, int which) {
						mIsDoingAdjustment = false;
						dialog.dismiss();
					}
					
				});
				builder.setNegativeButton("Adjusted", new DialogInterface.OnClickListener(){

					@Override
					public void onClick(DialogInterface dialog, int which) {
						isCheckPostiion = true;
						dialog.dismiss();
					}
					
				});
				builder.create().show();
			}
		}
		
	}
	
	/** wrong password for AP **/
	private void wrongKeyForAP(String ssid){
		if(mAPConnectedFlag || mRemoteAPSSID == null)
			return;
		if(	mUIState == UIState.STATE_UI_SHOW_CHOICE_FOR_AP
				|| mUIState == UIState.STATE_UI_CANCEL_CHOICE_FOR_AP
				|| mUIState == UIState.STATE_UI_CANCEL_WRONG_PASSWORD 
				|| mUIState == UIState.STATE_UI_CANCEL_CONNECTING_AP)
			return;
		
		mUIState = UIState.STATE_UI_SHOW_WRONG_PASSWORD;

		if(isCheckPostiion){
			isCheckPostiion = false;
			//Log.d(TAG,"!!! stopWaiting !!! wrongKeyForAP");
			stopWaiting();
			
			AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
			builder.setTitle("Warning");
			builder.setMessage("The password is wrong!");
			builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
				
			});
			builder.create().show();
		}
		
		
	}
	
	/** connecting to AP **/
	private void connectToAPFail(){
		if(mAPConnectedFlag || mRemoteAPSSID == null)
			return;
		if(mUIState == UIState.STATE_UI_SHOW_CHOICE_FOR_AP
				|| mUIState == UIState.STATE_UI_CANCEL_CHOICE_FOR_AP 
				|| mUIState == UIState.STATE_UI_SHOW_WRONG_PASSWORD
				|| mUIState == UIState.STATE_UI_CANCEL_WRONG_PASSWORD 
				|| mUIState == UIState.STATE_UI_CANCEL_CONNECTING_AP
				|| mUIState == UIState.STATE_UI_SHOW_CHOICE_FOR_ADJUSTMENT
				|| mUIState == UIState.STATE_UI_DISMISS_CHOICE_FOR_ADJUSTMENT)
			return;
		
		if(isCheckPostiion){
			isCheckPostiion = false;
			//Log.d(TAG,"!!! stopWaiting !!! connectToAPFail");
			stopWaiting();
			
			AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
			builder.setTitle("Error");
			builder.setMessage("Connection Fail!");
			builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
				
			});
			builder.create().show();
		}
		
	}
	
	private byte[] constructAPProfile(byte band, byte encrypt, String ssid, String mac,String pwd){
		byte[] mAPProfile = new byte[104];
		
		byte[] mAPSsid_bytes = ssid.getBytes();
		byte[] mAPMac_bytes = MacToByteArray(mac);
		
		mAPProfile[0] = band;
		mAPProfile[1] = encrypt;
		
		System.arraycopy(mAPSsid_bytes, 0, mAPProfile, 2, mAPSsid_bytes.length);
		System.arraycopy(mAPMac_bytes, 0, mAPProfile, 34, mAPMac_bytes.length);	
		if(encrypt != 0){
			byte[] mAPPwd = pwd.getBytes();
			System.arraycopy(mAPPwd, 0, mAPProfile, 40, mAPPwd.length);
		}
		
		return mAPProfile;
	}//constructAPProfile
	/** convert MAC in 00:00:00:00:00:00 format to byte[hex] format **/
	private byte[] MacToByteArray(String hex_str){
    	String[] hex = hex_str.split(":");
    	byte[] returnBytes = new byte[hex.length];
    	for(int i = 0; i < hex.length; i++){
    		returnBytes[i] = (byte)Integer.parseInt(hex[i].substring(0), 16);
    	}
    	return returnBytes;
    }

	public void startWaiting(String title, String message, int timeout, String timeout_msg) 
	{
		Log.d(TAG,"!!! startWaiting !!! "+timeout+ " "+title+" "+message);
		if(pd!=null){
			if(pd.isShowing()){
				pd.dismiss();
				
				try {
					Thread.sleep(400);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		
		pd = new ProgressDialog(MainActivity.this);
		pd.setTitle(title);
		pd.setMessage(message);
		pd.setProgressStyle(ProgressDialog.STYLE_SPINNER);
		pd.setIndeterminate(true);
		pd.setCancelable(false);
		pd.setButton(DialogInterface.BUTTON_NEGATIVE, "Cancel",
				new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						if(deviceList_alert!=null){
							deviceList_alert.cancel();
							deviceList_alert = null;
						}
					}
				});
		pd.show();
		
		
		if(wait_timer != null){
			wait_timer.cancel();
			wait_timer = null;
		}
		wait_timer = new Timer(true);
		WaitTimerStopTask timerTask = new WaitTimerStopTask();
		timerTask.setMsg(timeout_msg);
		
		if(timeout>0){
			wait_timer.schedule(timerTask, timeout*1000);
		}else{
			wait_timer.schedule(timerTask, 3600*1000);
		}
	}
	
	public void stopWaiting()
	{
		Log.d(TAG,"!!! stopWaiting !!!");
		handler_pd.sendEmptyMessage(0);

		if(wait_timer != null){
			wait_timer.cancel();
			wait_timer = null;
		}
		
	}
	
	public class WaitTimerStopTask extends TimerTask
	{
		private String msg = "";
		
		public void setMsg(String arg) {
			msg = arg;
		}
		
		public void run(){
			Log.d(TAG,"!!! stopWaiting !!! WaitTimerStopTask");
			
			if(wait_timer != null && msg.length()>0){

				runOnUiThread(new Runnable() {
					@Override
					public void run() {
						AlertDialog.Builder builder = new AlertDialog.Builder(mMainAct);
						builder.setTitle("Warning");
						builder.setMessage(msg);
						builder.setPositiveButton("OK", new DialogInterface.OnClickListener(){

							@Override
							public void onClick(DialogInterface dialog, int which) {
								msg = "";
								if(deviceList_alert!=null){
									deviceList_alert.cancel();
									deviceList_alert = null;
								}
								
								dialog.dismiss();
							}
						});
						builder.create().show();
					}
				});
				
				
			}
			
			stopWaiting();
		}
	};
	
 
	
}
