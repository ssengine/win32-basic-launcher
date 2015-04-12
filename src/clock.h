#pragma once

struct clock{
public:
    virtual ~clock(){};
    virtual double get_dt(bool peek = false) = 0;
    virtual bool is_valid() = 0;
};

struct hr_clock : clock{
    hr_clock(){
        LARGE_INTEGER freq;
        valid = QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&stamp);
        frequency = (double)freq.QuadPart;
    }
    virtual ~hr_clock(){

    }
    virtual double get_dt(bool peek = false){
        LARGE_INTEGER newT;
        QueryPerformanceCounter(&newT);

        double ret = (newT.QuadPart - stamp.QuadPart) / frequency;
        if (!peek){
            stamp = newT;
        }
        return ret;
    }
    virtual bool is_valid(){
        return valid;
    }
private:
    bool valid;
    double frequency;
    LARGE_INTEGER stamp;
};

struct lr_clock : clock{
    lr_clock(){
        stamp = GetTickCount();
    }
    virtual ~lr_clock(){

    }
    virtual double get_dt(bool peek = false){
        DWORD newT = GetTickCount();
        double ret = (newT - stamp) / 1000.0;
        if (!peek){
            stamp = newT;
        }
        return ret;
    }
    virtual bool is_valid(){
        return true;
    }
    DWORD stamp;
};
