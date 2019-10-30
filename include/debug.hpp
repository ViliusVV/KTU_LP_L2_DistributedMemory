#define logger(x) {\
    char buff[512];\
    std::string msg(x);\
    snprintf(buff,512, "[ProcID:%-1d] [Func:%13s] [Line:%-3d] %s", MPI::COMM_WORLD.Get_rank(), __FUNCTION__,__LINE__, msg.c_str());\
    std::string strBuff(buff);\
    std::cout << strBuff << std::endl;\
    }

