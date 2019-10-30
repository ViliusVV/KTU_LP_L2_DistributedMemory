#ifndef THREAD_SAFE_COUT
#define THREAD_SAFE_COUT

#include <iostream>
#include <sstream>
#include <mutex>

#define terr ThreadStream(std::cerr)
#define tout ThreadStream(std::cout)

class ThreadStream : public std::ostringstream
{
    public:
        ThreadStream(std::ostream& os) : os_(os)
        {
            imbue(os.getloc());
            precision(os.precision());
            width(os.width());
            setf(std::ios::fixed, std::ios::floatfield);
        }

        ~ThreadStream()
        {
            std::lock_guard<std::mutex> guard(_mutex_threadstream);
            os_ << this->str();
        }

    private:
        static std::mutex _mutex_threadstream;
        std::ostream& os_;
};


#endif