//
//  TFAudioPowerGraphView.m
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/12.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFAudioPowerGraphView.h"
#import "TFMPDebugFuncs.h"

static CGFloat pointSpace = 2;

@interface TFAudioPowerGraphView ()<UIScrollViewDelegate>{
    UIScrollView *_scrollView;
//    TFAudioPowerGraphContentView *_contentView;
    
//    NSMutableArray *_savedDatas;
    
    NSTimer *_timer;
    
    int _lastSampleCount;
    
    NSMutableArray<TFAudioPowerGraphContentView *> *_showedViews;
    NSMutableSet<TFAudioPowerGraphContentView *> *_unuseViews;
    
    NSRange _showingRange;
    NSInteger _curIndex;
    
    double _sampleValueMax;
    
    int _averageCount;
    int64_t _average;
    
    int _colorFlag;
    
    BOOL _canDraw;
}

@property (nonatomic, assign) CGFloat moveSpeed;

@property (nonatomic, assign) double refreshDuration;

@end

@implementation TFAudioPowerGraphView

-(instancetype)initWithFrame:(CGRect)frame{
    if (self = [super initWithFrame:frame]) {
        
        _moveSpeed = 1000;
        _showedViews = [[NSMutableArray alloc] init];
        _unuseViews = [[NSMutableSet alloc] init];
        _canDraw = YES;
        _showRate = 1;
        _colorFlag = 0;
        _colorFlagCycleCount = 1;
        
        _sampleValueMax = 1 << (sizeof(SInt16)*8-1);
        
        _bytesPerSample = sizeof(SInt16);
        
        _scrollView = [[UIScrollView alloc] initWithFrame:self.bounds];
        _scrollView.delegate = self;
        [self addSubview:_scrollView];
        
//        int count = _scrollView.bounds.size.width / pointSpace+1;
//        for (int i = 0; i<count; i++) {
//            TFAudioPowerGraphContentView *cell = [[TFAudioPowerGraphContentView alloc]initWithFrame:CGRectMake(i*pointSpace, 0, pointSpace, _scrollView.bounds.size.height)];
//            cell.horizontal = NO;
//            cell.id = i;
//            cell.backgroundColor = [UIColor whiteColor];
//            
//            [_scrollView addSubview:cell];
//            [_showedViews addObject:cell];
//        }
        
        _curIndex = 0;
//        _showingRange = NSMakeRange(0, count);
    }
    
    return self;
}

-(void)showBuffer:(void *)buffer size:(uint32_t)size{
    
    if (!_canDraw || buffer == nil) {
        return;
    }
    
    int sampleCount = size / _bytesPerSample;
    
    int frequency = _sampleRate / (_moveSpeed/pointSpace); //n个样本显示一个
    
    
    //暂时按s16来读取
    SInt16 *s16Buffer = buffer;
    if (_changeColor) {
        _colorFlag ++;
        if (_colorFlag == 2*_colorFlagCycleCount) {
            _colorFlag = 0;
        }
    }
    
    for (int i = 0; i<sampleCount; i++) {
        SInt16 rawValue = *s16Buffer;
        
        _average += rawValue;
        _averageCount++;
        
        if (_averageCount == frequency) {
//            printf("index: %d",i);
            [self showNextData:_average/(double)frequency/_sampleValueMax];
            _average = 0;
            _averageCount = 0;
        }
        
        s16Buffer++;
    }
}

-(void)showNextData:(float)value{
    value *= _showRate;
//    printf("<< %.6f\n",value);
    
    TFAudioPowerGraphContentView *cell = nil;
    if (NSLocationInRange(_curIndex, _showingRange)) {
        cell = _showedViews[_curIndex-_showingRange.location];
    }else{
        cell = [_unuseViews anyObject];
        if (cell) [_unuseViews removeObject:cell];
        
        if (cell == nil) {
            cell = [[TFAudioPowerGraphContentView alloc]initWithFrame:CGRectMake(_curIndex*pointSpace, 0, pointSpace, _scrollView.bounds.size.height)];
            cell.id = _curIndex;
            cell.ignoreSign = _ignoreSign;
        }
        cell.horizontal = NO;
        
        if (_changeBGColor) {
            cell.fillColor = [UIColor whiteColor];
            cell.backgroundColor = (_colorFlag >= _colorFlagCycleCount) ? [UIColor blackColor] : [UIColor colorWithRed:1 green:0.9 blue:0.8 alpha:1];
        }else{
            cell.backgroundColor = [UIColor whiteColor];
            cell.fillColor = (_colorFlag >= _colorFlagCycleCount) ? [UIColor blackColor] : [UIColor colorWithRed:1 green:0.9 blue:0.8 alpha:1];
        }
        
        
        cell.frame = CGRectMake(_curIndex*pointSpace, 0, pointSpace, _scrollView.bounds.size.height);
        
        
        _showingRange.length += 1;
        [_showedViews addObject:cell];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [_scrollView addSubview:cell];
            _scrollView.contentSize = CGSizeMake((_showingRange.length+_showingRange.location)*pointSpace, _scrollView.bounds.size.height);
        });
    }
    
    cell.value = value;
    _curIndex++;
}

-(void)start{
    _canDraw = YES;
}

-(void)stop{
    _canDraw = NO;
}

-(void)scrollGraph{
    
    NSInteger showCount = _scrollView.bounds.size.width / pointSpace+1;
    NSInteger valuedCount = _curIndex - _showingRange.location;
    
    if (valuedCount < showCount) {
        return;
    }
    
    NSInteger dropCount = valuedCount - showCount;
    
    NSInteger showFirstIndex = _showingRange.location+dropCount;
    
//    for (NSInteger i = 0; i<dropCount; i++) {
//
//        TFAudioPowerGraphContentView *cell = _showedViews[i];
//
//        [cell removeFromSuperview];
//
//        [_unuseViews addObject:cell];
//    }
    
    [_showedViews removeObjectsInRange:NSMakeRange(0, dropCount)];
    
    _showingRange.location = showFirstIndex;
    _showingRange.length -= dropCount;
    
    _scrollView.contentOffset = CGPointMake(MAX(_scrollView.contentSize.width - _scrollView.bounds.size.width, 0), 0);
    
}

@end


#pragma mark -

@implementation TFAudioPowerGraphContentView

-(void)setValue:(float)value{
    
    _value = value;
    [self setNeedsDisplay];
}

-(void)drawRect:(CGRect)rect{
    
    CGContextRef context = UIGraphicsGetCurrentContext();
    
    CGFloat width = rect.size.width, height = rect.size.height;
    
    CGRect fillRect;
    if (_ignoreSign) {
        
        float ignoreSignValue = fabsf(_value);
        
        if (_horizontal) {
            
            fillRect = CGRectMake((1-ignoreSignValue)*width/2.0, 0, ignoreSignValue*width, height);
            
        }else{
            fillRect = CGRectMake(0, (1-ignoreSignValue)*height/2.0, width, ignoreSignValue*height);
        }
        
    }else{
        if (_horizontal) {
            if (_value < 0) {
                fillRect = CGRectMake((1+_value)*width/2.0, 0, -_value*width/2.0, height);
            }else{
                fillRect = CGRectMake(width/2.0, 0, _value*width/2.0, height);
            }
            
        }else{
            if (_value < 0) {
                fillRect = CGRectMake(0, height/2.0, width, -_value*height/2.0);
            }else{
                fillRect = CGRectMake(0, (1-_value)*height/2.0, width, _value*height/2.0);
            }
        }
    }
    
    CGContextSetFillColorWithColor(context, _fillColor.CGColor);
    CGContextFillRect(context, fillRect);
}

@end
