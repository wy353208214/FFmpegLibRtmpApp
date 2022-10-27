//
//  DetailController.swift
//  MacVideoPlayer
//
//  Created by steven on 2022/10/9.
//

import Cocoa
import SnapKit

class DetailController: NSViewController {
    
    override func loadView() {
        self.view = NSView(frame: CGRect(x: 0, y: 0, width: 480, height: 320))
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.
        
        self.view.wantsLayer = true
        self.view.layer?.backgroundColor = NSColor.red.cgColor
        
        
        let content = NSText.init()
        content.string = "我是一个NStext"
        content.textColor = NSColor.blue
        content.alignment = .center
        content.backgroundColor = NSColor.orange
        
        //
        let label = NSTextField.init()
        label.stringValue = "第二个controller"
        label.isEditable = false
        label.alignment = .center
        label.maximumNumberOfLines = 5
        
    
        view.addSubview(label)
        view.addSubview(content)
        
        label.snp.makeConstraints{(make) -> Void in
            make.centerX.equalToSuperview()
            make.top.equalToSuperview().offset(10)
        }
        
        content.snp.makeConstraints{(make) -> Void in
            make.centerX.equalTo(label)
            make.size.equalTo(label)
            make.top.equalTo(label.snp.bottom).offset(10)
        }
        
        let gesture = NSClickGestureRecognizer()
        gesture.buttonMask = 0x1 // left mouse
        gesture.numberOfClicksRequired = 1
        gesture.target = self
        gesture.action = #selector(back)
        label.addGestureRecognizer(gesture)
        
    }
    
    @objc
    func back(){
        if (parent != nil) {
            parent!.transition(from: self, to: parent!.children[0], options: .crossfade, completionHandler: nil)
        }
    }
    
}
