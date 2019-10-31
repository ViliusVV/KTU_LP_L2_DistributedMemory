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
#define ROOT_PROCESS 0
#define DATA_PROCESS 1
#define RESULT_PROCESS 2
#define ENTRY_CNT_MAX 250

#define JSON_DATA_FILE "data/IFF7-4_ValinskisV_L1_dat_1.json"

// Alias
using json = nlohmann::json;

// Tag enumerator to make MPI message tag readabilty better
enum TagEnum
{
    dataTag = 0,
    statusTag
};

// Custom status enumerator to eliminate need of diferent tags for each flag
enum StatusEnum {
  clear         = 0x00, // 0000 0000
  isNotEmpty    = 0x01, // 0000 0001
  isNotFull     = 0x02, // 0000 0010
  ready         = 0x04, // 0000 0100
  makeItStop    = 0x08, // 0000 1000
  ddd           = 0x10, // 0001 0000
  dd            = 0x20, // 0010 0000
  placeHolder2  = 0x40, // 0100 0000
  placeHolder3  = 0x80  // 1000 0000
};  

// Function prototypes
std::string heavyFunction(Person p);

int deserializeJsonFile(std::string fileName, Person arr[]);
void saveToFile(Person outArr[], int outCnt);

void MPI_rootProcess();
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
    switch (mpi_rank)
    {
    // =================================================================
    case ROOT_PROCESS:
        // Guard check - make sure there are at least 3 processes running
        if (mpi_size < 3)
        {
            std::cerr << "Require at least 3 processes" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1); //
        }

        logger("Root process started!");
        MPI_rootProcess();
        break;
    // =================================================================
    case DATA_PROCESS:
        logger("Data process started!");
        MPI_dataProcess();
        break;
    // =================================================================
    case RESULT_PROCESS:
        logger("Result process started!");
        MPI_resultProcess();
        break;
    // =================================================================
    default:
        logger("Worker process started!");
        MPI_workerProcess();
        break;
        // =================================================================
    }
    // End MPI segment
    MPI::Finalize();

    return 0;
}


// ===============================================================
// ===================== PROCESS FUNCTIONS =======================
// ===============================================================

// Main MPI process
void MPI_rootProcess()
{
    // Local arrya to store people
    Person people[ENTRY_CNT_MAX];
    int peopleCount = deserializeJsonFile(JSON_DATA_FILE, people);

    std::cout << people[0].InfoHeader();
    for (auto i = 0; i < peopleCount; i++)
    {
        std::cout << people[i];
    }

    std::cout << "Total count:" << peopleCount << std::endl;

    std::string jsonStr = people[0].Serialize();
    std::cout << "Serialized JSON: " << jsonStr << std::endl;
    Person tmp(jsonStr);
    std::cout << "Desiarilzed JSON: " << tmp << std::endl;
    
    StatusEnum status = clear;

    MPI::COMM_WORLD.Recv(&status, 1, MPI::SHORT, DATA_PROCESS, statusTag);
    if(status & isNotFull)
    {
        logger("Data process is not full.")
    }

}


// Function to call procedures specific to data process. It reads data and
// distributes to worker procceses
void MPI_dataProcess()
{

}

// Function to call procedures specifijc to result process.It waits for worker processes
// to send their finished work and puts into result data structure
void MPI_resultProcess()
{
    Person *outPeople = new Person[ENTRY_CNT_MAX]();
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
    for (auto i = 1; i < outCnt; i++)
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
    if (person.StreetNum / 100 < 1.0)
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
