#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <string>
#include <chrono>
#include <json.hpp>
#include "sha512.h"
#include "person.hpp"
#include "monitor.hpp"
#include <atomic>
#include <threadSafeCout.hpp>
#include <debug.hpp>
#define THREAD_CNT 8
#define OUT_ARR_SIZE 250
using json = nlohmann::json;

std::string heavyFunction(Person p);
void saveToFile(Person outArr[], int outCnt);

int entryCount = 0;
std::atomic<int> counter = 0;

std::mutex ThreadStream::_mutex_threadstream{}; // For thread safe cout

int main()
{
    std::cout << "Start V0.0.19..." << std::endl;

    static Monitor monitor;

    std::vector<Person> people;
    Person outPeople[OUT_ARR_SIZE];
    std::atomic<int> outCount = 0;

    // Import json file
    std::ifstream i("data/IFF7-4_ValinskisV_L1_dat_1.json");

    // Create json object
    json j;
    i >> j;

    // Transfer json data to Person vector
    for (auto &x : j.items())
    {
        Person tmpPerson;

        tmpPerson.Name = x.value()["Name"].get<std::string>();
        tmpPerson.StreetNum = x.value()["StreetNum"].get<int>();
        tmpPerson.Balance = x.value()["Balance"].get<double>();
        people.push_back(tmpPerson);
        entryCount++;
    }

    std::vector<std::thread> threads;
    threads.reserve(THREAD_CNT + 1);

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    // Spawn threads for taking out elemets from monitor and executing time heavy task
    for (int i = 0; i < THREAD_CNT; i++)
    {
        threads.emplace_back([&] {
            while (!monitor.get_finished())
            {
                Person tmpPerson = monitor.pop();
                if(tmpPerson.isNull())
                {
                    monitor.notifyFinish();
                    break;
                }
                tmpPerson.HahsValue = heavyFunction(tmpPerson);
                outCount = monitor.SaveResToVec(outPeople, tmpPerson);
            }
        });
    }

    // Insert elemets into monitor
    threads.emplace_back([&] {
        for (int i = 0; i < entryCount; i++)
        {
            Person tmpPerson;
            tmpPerson.Clone(people[i]);
            monitor.insert(tmpPerson);
            dlog("Inserted " + std::to_string(i) + " th element");
        }
        monitor.set_finished();
        dlog("FINISHED INSERTING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
    });

    // Join all threads
    for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

    // Print results to screen
    std::cout << outPeople[0].InfoHeader();
    for(auto i = 1; i < outCount; i++)
    {
        std::cout << outPeople[i];
    }

    // Print out elapsed time
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout   << "Elapsed time with " << THREAD_CNT << " threads:" 
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0 
                << "[sec]" << std::endl;
    
    
    std::cout << "Count: " << outCount << std::endl;
    std::cout << "Total: " << entryCount << std::endl;
    std::cout << "Filtered out: " << entryCount-outCount << std::endl;

    // Write results to file
    saveToFile(outPeople,outCount);

    return 0;
}

void saveToFile(Person outArr[], int outCnt)
{
    std::ofstream ofs("IFF_7_4_ViliusV_L1_rez.txt");
    ofs << outArr[0].InfoHeader();
    for(auto i = 1; i < outCnt; i++)
    {
        ofs << outArr[i];
    }

    ofs.close();
}


// Time heavy function
std::string heavyFunction(Person p)
{
    std::string output = sha512(p.Name + std::to_string(p.StreetNum) + std::to_string(p.Balance));
    for (int i = 0; i < 250000; i++)
    {
        output = sha512(output);
    }
    return output;
}