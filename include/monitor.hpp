#ifndef MONITOR_H
#define MONITOR_H

#include <mutex>
#include <condition_variable>
#include <person.hpp>
#include <atomic>
#include <vector>

#define SIZE 32

class Monitor {
private:
    const int size = SIZE;
    Person mPeople[SIZE];

    std::mutex lock;
    std::mutex saveLock;
    std::condition_variable cv;

    bool finished;

    int count = 0;
    int extCount = 0;

public:
    Monitor();
    void insert(Person person);
    Person pop();
    void set_finished();
    bool get_finished();
    int SaveResToVec(Person outPeople[], Person person);
    void notifyFinish();
};

#endif