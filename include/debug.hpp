#ifndef LOG 
#define LOG 1 // set debug mode
#endif

#if LOG 
#include <iostream>
#include <thread>
#define dlog(x) {\
    std::thread::id tId = std::this_thread::get_id();\
    std::stringstream ss;\
    ss << tId;\
    uint64_t id = std::stoull(ss.str());\
    char buff[512];\
    std::string msg(x);\
    snprintf(buff,512, "[Func:%-13s] [Line:%-3d] [Thread:%-1lu] %s\n", __FUNCTION__,__LINE__,id, msg.c_str());\
    std::string strBuff(buff);\
    tout << strBuff;\
    }
#else
#define dlog(...)
#endif