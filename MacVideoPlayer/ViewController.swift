//
//  ViewController.swift
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

import Cocoa
import SnapKit
import AVFoundation

class ViewController: NSViewController {
    
    
    let detailController = DetailController()
    let mainController = MainController()
    
    override func viewDidLoad() {
        super.viewDidLoad()
        self.view.setFrameSize(NSSize(width: 480, height: 320))
    
        addChild(mainController)
        addChild(detailController)
        
        // 添加显示的子view
        self.view.addSubview(mainController.view)

    }

    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }


}

