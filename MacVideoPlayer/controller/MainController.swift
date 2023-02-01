//
//  MainController.swift
//  MacVideoPlayer
//
//  Created by steven on 2022/10/11.
//

import Cocoa
import SnapKit
import AVFoundation


enum UserEvent {
    case Record_Video
    case Record_Audio
    case Record_ALL
    case Convet_AAC
    case Stop_Record
    case PUSH_STREAM
    case Play
}

class MainController: NSViewController{
    
    private var events: NSArray = [UserEvent.Record_Video, UserEvent.Record_Audio, UserEvent.Record_ALL, UserEvent.Convet_AAC, UserEvent.Stop_Record, UserEvent.PUSH_STREAM, UserEvent.Play]
    private var datas: NSArray = ["录制视频", "录制音频", "录取音视频", "PCM转化AAC", "停止录制", "推流", "播放视频"]
    
    private let ff = OcMediaManager.init()
    
    
    override func loadView() {
        self.view = NSView(frame: CGRect(x: 0, y: 0, width: 480, height: 320))
    }
    
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        self.view.setFrameSize(NSSize(width: 480, height: 320))
        self.view.wantsLayer = true
        self.view.layer?.backgroundColor = NSColor.blue.cgColor
        
        
        let scrollView = NSScrollView.init()
        let collectionView = NSCollectionView.init()
        self.view.addSubview(scrollView)
        scrollView.documentView = collectionView
        
        collectionView.dataSource = self
        let viewLayout = NSCollectionViewFlowLayout.init()
        viewLayout.itemSize = NSSize.init(width: 100, height: 100)
        viewLayout.minimumLineSpacing = 2
        viewLayout.minimumInteritemSpacing = 2
        collectionView.collectionViewLayout = viewLayout
        
        scrollView.snp.makeConstraints{(make) -> Void in
            make.size.equalToSuperview()
        }
        
        collectionView.snp.makeConstraints{(make) -> Void in
            make.left.equalToSuperview().offset(2)
            make.right.equalToSuperview().offset(-2)
        }
        
        collectionView.register(MyCellItem.self, forItemWithIdentifier:  NSUserInterfaceItemIdentifier(rawValue: "Cell"))
        
    }
}

extension MainController: NSCollectionViewDataSource{
    
    
    func collectionView(_ collectionView: NSCollectionView, numberOfItemsInSection section: Int) -> Int {
        return datas.count
    }
    
    func collectionView(_ collectionView: NSCollectionView, itemForRepresentedObjectAt indexPath: IndexPath) -> NSCollectionViewItem {
        let cell = collectionView.makeItem(withIdentifier: NSUserInterfaceItemIdentifier(rawValue: "Cell"), for: indexPath) as! MyCellItem
        cell.view.wantsLayer = true
        cell.view.layer?.backgroundColor = NSColor.red.cgColor
        
        let label = cell.label
        label.stringValue = datas[indexPath.item] as! String
        label.tag = indexPath.item
        
        let gesture = NSClickGestureRecognizer()
        gesture.buttonMask = 0x1 // left mouse
        gesture.numberOfClicksRequired = 1
        gesture.target = self
        gesture.action = #selector(clickText)
        cell.view.addGestureRecognizer(gesture)
        
        return cell
    }
    
    @objc
    func clickText(sender: NSClickGestureRecognizer){
        let view = sender.view!
        let arrayViews = view.subviews
        let position = arrayViews[0].tag
        
        let event: UserEvent = events[position] as! UserEvent
        switch event{
        case .Record_Video:
            switch AVCaptureDevice.authorizationStatus(for: .video){
            case .authorized:
                ff.start(ORecordType.OC_VIDEO);
//                listDevice();
                break
            case .denied:
                NSLog("Request Camera denied")
                break
            case .notDetermined:
                NSLog("Request Camera notDetermined")
                break
            case .restricted:
                NSLog("Request Camera restricted")
                break
                
            }
        case .Record_Audio:
            ff.start(ORecordType.OC_AUDIO)
//            navigationController(nextController: parent!.children[1])
        case .Record_ALL:
            ff.start(ORecordType.OC_ALL)
            break;
        case .Stop_Record:
            ff.stopRecord()
        case .Convet_AAC:
            ff.convertPcm2AAC()
        case .PUSH_STREAM:
            ff.pushStream()
            
        case .Play:
//            ff.play("/Users/steven/Movies/Video/S8.mp4")
            ff.play("/Users/steven/Movies/Video/luoxiang.mp4")
        }
    }
    
    func navigationController(nextController: NSViewController) -> Void {
        //跳转到下一个界面
        if parent != nil {
            parent!.transition(from: self, to: nextController, options: .crossfade, completionHandler: nil)
        }
    }
    
    func checkPreset(session: AVCaptureSession) -> NSMutableArray {
        let array = NSMutableArray.init()
        let presets = [AVCaptureSession.Preset.high, AVCaptureSession.Preset.low, AVCaptureSession.Preset.medium,
                       AVCaptureSession.Preset.qHD960x540, AVCaptureSession.Preset.qvga320x240,
                       AVCaptureSession.Preset.hd1280x720, AVCaptureSession.Preset.hd1920x1080,
                       AVCaptureSession.Preset.hd4K3840x2160, AVCaptureSession.Preset.vga640x480]
        for preset in presets {
            if (session.canSetSessionPreset(preset)) {
                array.add(preset)
            }
        }
        return array
    }
    
    func listDevice() -> Void {
        //计算屏幕尺寸，ScrennCapture
        let screen = NSScreen.main
        print(screen!.frame)
        
        //查看外接设备基本信息
        let disCoverySession = AVCaptureDevice.DiscoverySession.init(deviceTypes: [AVCaptureDevice.DeviceType.externalUnknown], mediaType: .video, position: .unspecified)
        let devices = disCoverySession.devices
        for device in devices {
            print(device.localizedName)
            for format in device.formats {
                //                        print(format.formatDescription, format.videoSupportedFrameRateRanges)
                print(format)
            }
            
            //下面代码用来检测是否支持一些分辨率
            //                    let session = AVCaptureSession.init()
            //                    let devInput = try? AVCaptureDeviceInput.init(device: device)
            //                    if session.canAddInput(devInput!) {
            //                        session.addInput(devInput!)
            //                        print(device.localizedName, checkPreset(session: session))
            //                    }
        }
    }
    
}
