/*
 
 File: ViewController.h
 
 Abstract: View Controller to select whether the App runs in Central or 
 Peripheral Mode
 
 Version: 1.0
 
 Created by CN3_SD9 on 2016/9/28.
 Copyright © 2016年 Realtek. All rights reserved.
 
 */

#import <UIKit/UIKit.h>

@interface ViewController : UIViewController
{
    NSMutableArray *m_devlist;                              // BT device list
    NSMutableArray *m_APlist;                               // AP list
}

@property (nonatomic,strong) CBCentralManager *centralManager;

@property (strong, nonatomic) IBOutlet UIImageView  *img_bt_device_state;
@property (strong, nonatomic) IBOutlet UILabel      *label_btDevice;
@property (strong, nonatomic) IBOutlet UITableView  *discover_table;
@property (strong, nonatomic) IBOutlet UITableView  *myConnectTable;
@property (strong, nonatomic) IBOutlet UIButton     *btn_image_ss;
@property (strong, nonatomic) NSMutableArray        *dev_array;


@property (strong, nonatomic) IBOutlet UILabel *target_SSID;
@property (strong, nonatomic) IBOutlet UILabel *target_BSSID;
@property (strong, nonatomic) IBOutlet UIImageView *target_security;
@property (strong, nonatomic) IBOutlet UIImageView *target_RSSI;

@end
