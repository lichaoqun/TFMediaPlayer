//
//  RecycleBuffer.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef RecycleBuffer_hpp
#define RecycleBuffer_hpp

#include <stdio.h>
#include <pthread.h>
#include <limits.h>

/* |->--front-----back------>| */

namespace tfmpcore {
    template<typename T>
    class RecycleBuffer{
        
        class RecycleNode{
            T val;
            struct RecycleNode *pre;
            struct RecycleNode *next;
            
            friend RecycleBuffer;
        };
        
        long limitSize = LONG_MAX;
        long allocedSize = 0;
        long usedSize = 0;
        
        RecycleNode *frontNode = nullptr;  //the newest used node
        RecycleNode *backNode = nullptr;  // the oldest used node
        
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        
        bool expand(){
            if (allocedSize >= limitSize) {
                return false;
            }
            
            long expandSize = allocedSize < (limitSize-allocedSize) ? allocedSize : (limitSize-allocedSize);
            
            RecycleNode *nextNode = frontNode;
            RecycleNode *closeNode = frontNode->pre;
            
            //construct from next to pre.  new node(pre) <----- front(next).
            for (long i = 0; i<expandSize; i++) {
                RecycleNode *node = new RecycleNode();
                nextNode->pre = node;
                node->next = nextNode;
                
                if (i == expandSize-1) { //link last node with the other node of break.
                    node->pre = closeNode;
                    closeNode->next = node;
                }
                
                nextNode = node;
            }
            
            allocedSize += expandSize;
            
            return true;
            
            
        }
        
    public:
        
        RecycleBuffer(long limitSize = 0, bool allocToLimit = false){
            if (limitSize > 0) {
                this->limitSize = limitSize;
            }
            
            if (limitSize && allocToLimit) {
                
                allocedSize = limitSize;
            }else{
                allocedSize = 8; //init size
            }
            
            
            frontNode = new RecycleNode();
            RecycleNode *lastNode = frontNode;
            
            for (long i = 1; i<allocedSize; i++) {
                RecycleNode *node = new RecycleNode();
                
                lastNode->next = node;
                node->pre = lastNode;
                
                if (i == allocedSize-1) { //link last node with the other node of break.
                    node->next = frontNode;
                    frontNode->pre = node;
                }
                
                lastNode = node;
            }
            
            backNode = frontNode->pre;
        }
        
        /** name for identifying this RecycleNode */
        char name[16];
        
        /** Use this func to free RecycleNode.val as RecycleBuffer doesn't know what T exactly is. */
        void (*valueFreeFunc)(T *val);
        
        bool isFull(){
            return usedSize == limitSize;
        };
        bool isEmpty(){
            return usedSize == 0;
        }
        
        bool insert(T val){
            if (usedSize == allocedSize) {
                if (!expand()) {
                    //stop pushing until there is unused node.
                    return false;
                }
            }
            
            frontNode->pre->val = val;
            frontNode = frontNode->pre;
            
            usedSize++;
            printf("%s: %ld\n",name,usedSize);
            
            return true;
        }
        
        bool getOut(T *valP){
            
            if (usedSize == 0) {
                return false;
            }
            
            if (valP) *valP = backNode->val;
            
            backNode = backNode->pre;
            
            usedSize--;
            printf("get out\n");
            
            return true;
        }
        
        void blockInsert(T val){
            
            if (usedSize >= limitSize) {
                printf(">>>>lock full %s\n",name);
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&cond, &mutex);
                pthread_mutex_unlock(&mutex);
                printf("<<<<unlock full %s\n",name);
            }
            
            insert(val);
            pthread_cond_signal(&cond);
        }
        
        void blockGetOut(T *valP){

            if (usedSize == 0) {
                
//                printf(">>>>lock empty %s\n",identifier);
                
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&cond, &mutex);
                pthread_mutex_unlock(&mutex);
                
//                printf("<<<<unlock empty %s\n",identifier);
            }
            
            getOut(valP);
            pthread_cond_signal(&cond);
        }
        
        bool back(T *valP){
            if (usedSize == 0) {
                return false;
            }
            *valP = backNode->val;
            
            return true;
        }
        
        bool front(T *valP){
            if (usedSize == 0) {
                return false;
            }
            *valP = frontNode->val;
            
            return true;
        }
        
        void signalAllBlock(){
            pthread_cond_broadcast(&cond);
        }
        
        void clear(){
            pthread_mutex_lock(&mutex);
            
            //free valid datas
            if (usedSize > 0 && valueFreeFunc != nullptr) {
                RecycleNode *curNode = frontNode;
                do {
                    valueFreeFunc(&(curNode->val));
                    curNode = curNode->next;
                } while (curNode != backNode->next);
            }
            
            //free all nodes
            RecycleNode *curNode = frontNode;
            do {
                RecycleNode *next = curNode->next;
                free(curNode);
                curNode = next;
            } while (curNode != backNode->next);
            
            frontNode = nullptr;
            backNode = nullptr;
            
            allocedSize = 0;
            usedSize = 0;
            limitSize = LONG_MAX;
            
            printf("free %s\n",name);
            
            pthread_mutex_unlock(&mutex);
        }
    };
}

#endif /* RecycleBuffer_hpp */
