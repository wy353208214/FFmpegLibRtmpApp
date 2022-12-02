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
    case Convet_AAC
    case Stop_Record
    case PUSH_STREAM
}

class MainController: NSViewController{
    
    private var events: NSArray = [UserEvent.Record_Video, UserEvent.Record_Audio, UserEvent.Convet_AAC, UserEvent.Stop_Record, UserEvent.PUSH_STREAM]
    private var datas: NSArray = ["录制视频", "录制音频", "PCM转化AAC", "停止录制", "推流"]
    
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
                print("点击的是：", datas[position])
                ff.start(ORecordType.OC_VIDEO);
            case .Record_Audio:
                print("点击的是：", datas[position])
                
            case .Stop_Record:
                print("点击的是：", datas[position])
                ff.stopRecord()
            case .Convet_AAC:
                print("点击的是：", datas[position])
                ff.convertPcm2AAC()
            case .PUSH_STREAM:
                ff.pushStream()
                print("点击的是：", datas[position])
        }
    }
    
}
