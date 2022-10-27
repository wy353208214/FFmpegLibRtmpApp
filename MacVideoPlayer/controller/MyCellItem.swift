//
//  MyCellItem.swift
//  MacVideoPlayer
//
//  Created by steven on 2022/10/21.
//

import Cocoa

class MyCellItem: NSCollectionViewItem {
    
    let label = NSTextField.init()
    
    override func loadView() {
        self.view = NSView()
        self.view.wantsLayer = true
        self.view.layer?.backgroundColor = NSColor.clear.cgColor
    
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        
        view.addSubview(label)
        
        label.isEditable = false
        label.alignment = .center
        label.maximumNumberOfLines = 3
        label.isBordered = false
        label.backgroundColor = NSColor.clear
        
        label.snp.makeConstraints{(make) -> Void in
            make.width.equalToSuperview()
            make.center.equalToSuperview()
        }
        
    }
    
}
