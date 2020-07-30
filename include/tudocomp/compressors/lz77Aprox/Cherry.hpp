#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/AproxFactor.hpp>
#include <vector>
namespace tdc{

class Cherry : public AproxFactor{
public:
    /*  Left and right Vector which holds information about the chains
     *  of the cherry
     */
    std::vector<bool> vectorL ,vectorR;
    //which children are found
    bool leftfound, rightfound;
    Cherry(len_t position, len_t length, int flag):AproxFactor(position,length,flag){
        leftfound=false;
        rightfound=false;
    }
    
    //return the rightchild and turns this into leftchild
    Cherry split(){
        Cherry rightchild = Cherry(position + length/2,length/2,0);
        //ihope this coyps
        rightchild.vectorR = vectorR;
        //change this
        vectorR.clear();
        length=length/2;
        //return
        return rightchild;
    }
    void only_leftfound(){
        vectorL.push_back(true);
        vectorR.push_back(false);
        position = position +length/2;
        length=length/2;
    }
    void only_rightfound(){
        vectorL.push_back(false);
        vectorR.push_back(true);
        length=length/2;
    }

};

}