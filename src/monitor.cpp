#include "monitor.hpp"
#include <limits>
#include "debug.hpp"
#include "threadSafeCout.hpp"

Monitor::Monitor()
{
    finished = false;
    extCount = 0;
}

void Monitor::insert(Person person)
{
    dlog("Acquiring lock....");
    std::unique_lock<std::mutex> guard(lock);
    dlog("Acquired lock!");
    dlog("Waiting CV...");
    // Wait till element count is less than buffer size
    cv.wait(guard, [this] { return count < size; });
    dlog("Adding person....");
    // Insert person at the end of buffer, and increment count
    mPeople[count++].Clone(person);
    dlog("Unlocking mutex...");
    guard.unlock();
    // Notify single waiting thread
    dlog("Notifyng one thread...");
    cv.notify_one();
}

Person Monitor::pop()
{
    Person tmpPerson;
    dlog("Acquiring lock....");
    std::unique_lock<std::mutex> guard(lock);
    dlog("Acquired lock!");
    dlog("Waiting CV...");
    cv.wait(guard, [this] { return count > 0 || count <= 0 && finished; });
    if(count <= 0 && finished)
    {
        guard.unlock();
        cv.notify_one();
         return {};
    }
    dlog("Popping person.... Left: " + std::to_string(count-1));
    // Copy person to temp object, no need to delete, it will be overwritten
    tmpPerson.Clone(mPeople[--count]);
    dlog("Unlocking mutex...");
    guard.unlock();
    cv.notify_one();
    dlog("Notifyng one thread...");
    return tmpPerson;
}

void Monitor::notifyFinish()
{
    dlog("Notify Finish.......");
    cv.notify_all();
}

int Monitor::SaveResToVec(Person outPeople[], Person person)
{
    std::lock_guard<std::mutex> guard(saveLock);
    // Filter: street number is 3 digit or more
    if(person.StreetNum/100 < 1.0)
    {
        return extCount;
    }

    // Sorted insert
    int i;
    for (i = extCount - 1; (i >= 0 && outPeople[i].Balance > person.Balance); i--)
        outPeople[i + 1] = outPeople[i];

    outPeople[i + 1] = person;

    return (++extCount);
}

void Monitor::set_finished()
{
    finished = true;
}

bool Monitor::get_finished()
{
    return finished && count <= 0;
}
