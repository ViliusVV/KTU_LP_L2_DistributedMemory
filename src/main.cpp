#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <string>
#include <chrono>
#include <json.hpp>
#include <debug.hpp>
#include </lib/x86_64-linux-gnu/openmpi/include/mpi.h>
#include "sha512.h"
#include "person.hpp"


#define LOG 1


// Custom defines
#define DATA_PROCESS 0
#define RESULT_PROCESS 1 
#define ENTRY_CNT_MAX 250

#define JSON_DATA_FILE "data/IFF7-4_ValinskisV_L1_dat_1.json"


using json = nlohmann::json;

// std::mutex ThreadStream::_mutex_threadstream{}; // For thread safe cout

// Tag enumerator to make MPI message tag readabilty better
enum Tag 
{
    workResult = 0,
    data,
    halt,
    print
};


// Function prototypes
std::string heavyFunction(Person p);

int deserializeJsonFile(std::string fileName, Person arr[]);
void saveToFile(Person outArr[], int outCnt);

void MPI_workerProcess();
void MPI_dataProcess();
void MPI_resultProcess();

int main()
{
    std::cout << "L2-Distributed-Memory. Start V1..." << std::endl;

    

    // Begin MPI segment
    MPI::Init();
    int mpi_size = MPI::COMM_WORLD.Get_size();
    int mpi_rank = MPI::COMM_WORLD.Get_rank();

    // Role specific code
    if(mpi_rank == DATA_PROCESS)
    {   
        // Guard check - make sure there are at least 3 processes running
        if (mpi_size < 3) 
        {
            std::cerr << "Require at least 3 processes" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1); // 
        }

        logger("Data process started!");
        MPI_dataProcess();
    }
    else if (mpi_rank == RESULT_PROCESS)
    {
        logger("Result process started!");
        MPI_resultProcess();
    }
    else  // Worker processes
    {
        logger("Worker process started!");
        MPI_workerProcess();
    }







    MPI::Finalize();

    return 0;
}

    // std::vector<std::thread> threads;
    // threads.reserve(THREAD_CNT + 1);

    // std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    // // Spawn threads for taking out elemets from monitor and executing time heavy task
    // for (int i = 0; i < THREAD_CNT; i++)
    // {
    //     threads.emplace_back([&] {
    //         while (!monitor.get_finished())
    //         {
    //             Person tmpPerson = monitor.pop();
    //             if(tmpPerson.isNull())
    //             {
    //                 monitor.notifyFinish();
    //                 break;
    //             }
    //             tmpPerson.HahsValue = heavyFunction(tmpPerson);
    //             outCount = monitor.SaveResToVec(outPeople, tmpPerson);
    //         }
    //     });
    // }

    // // Insert elemets into monitor
    // threads.emplace_back([&] {
    //     for (int i = 0; i < entryCount; i++)
    //     {
    //         Person tmpPerson;
    //         tmpPerson.Clone(people[i]);
    //         monitor.insert(tmpPerson);
    //         dlog("Inserted " + std::to_string(i) + " th element");
    //     }
    //     monitor.set_finished();
    //     dlog("FINISHED INSERTING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
    // });

    // // Join all threads
    // for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

    // // Print results to screen


    // // Print out elapsed time
    // std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    // std::cout   << "Elapsed time with " << THREAD_CNT << " threads:" 
    //             << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0 
    //             << "[sec]" << std::endl;
    
    

    // // Write results to file
    // saveToFile(outPeople,outCount);

    // return 0;


// ===============================================================
// ===================== PROCESS FUNCTIONS =======================
// ===============================================================


// Function to call procedures specific to data process. It reads data and
// distributes to worker procceses
void MPI_dataProcess()
{   
    // Local arrya to store people
    Person people[ENTRY_CNT_MAX];
    int peopleCount = deserializeJsonFile(JSON_DATA_FILE, people);

    std::cout << people[0].InfoHeader();
    for(auto i = 1; i < peopleCount; i++)
    {
        std::cout << people[i];
    }

    std::cout << "Total count:" << peopleCount << std::endl;
}


// Function to call procedures specifijc to result process.It waits for worker processes
// to send their finished work and puts into result data structure
void MPI_resultProcess()
{
    Person* outPeople = new Person[ENTRY_CNT_MAX]();
}


// Function to call procedures specific to worker procces. It waits untill data procces sends 
// message with data, then it will do computations with this data and afterwars it'll send to
// result process with modified data
void MPI_workerProcess()
{
    int p = 1;
}


// ===============================================================
// ========================== FILE FUNCTIONS =====================
// ===============================================================


// Takes json text file and deserializes to person data structure
int deserializeJsonFile(std::string fileName, Person arr[])
{   
    // Read json file
    std::ifstream i(JSON_DATA_FILE);

    // Create json object
    json j;
    i >> j;

    // Deserialize json
    int count = 0;
    for (auto &x : j.items())
    {
        Person tmpPerson;
        tmpPerson.Name = x.value()["Name"].get<std::string>();
        tmpPerson.StreetNum = x.value()["StreetNum"].get<int>();
        tmpPerson.Balance = x.value()["Balance"].get<double>();
        arr[std::stoi(x.key())] = tmpPerson;
        count++;
    }

    return count;
}


// Saves people data structure to text file
void saveToFile(std::string fileName, Person outArr[], int outCnt)
{
    std::ofstream ofs(fileName);
    ofs << outArr[0].InfoHeader(); // print out table header
    for(auto i = 1; i < outCnt; i++)
    {
        ofs << outArr[i];
    }

    ofs.close();
}


// ===============================================================
// ========================== MISC FUNCTIONS =====================
// ===============================================================


// Time heavy function which emulates in intensive task
std::string sha512Function(Person p)
{
    int iterationCount = 250000; // Hom many times to has
    std::string output = sha512(p.Name + std::to_string(p.StreetNum) + std::to_string(p.Balance));
    for (int i = 0; i < iterationCount; i++)
    {
        output = sha512(output);
    }
    return output;
}


int saveResToVec(Person outPeople[], Person person)
{
    int extCount;
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
