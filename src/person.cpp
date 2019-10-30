#include "person.hpp"
#include <stdio.h>
#include <string.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


// Constructors
Person::Person(){
    Name = "";
    StreetNum = 0;
    Balance = 0.0;
    HahsValue = "";
}


Person::Person(std::string name,int streetnum, double balance)
{
  Name = name;
  StreetNum = streetnum;
  Balance = balance;
  HahsValue = "";
}


Person::Person(std::string name,int streetnum, double balance, std::string hash)
{
  Name = name;
  StreetNum = streetnum;
  Balance = balance;
  HahsValue = hash;
}


// Serializes this object to json
std::string Person::Serialize()
{
  json jsn;

  jsn["Name"] = Name;
  jsn["StreetNum"] = StreetNum;
  jsn["Balance"] = Balance;
  jsn["HashValue"] = HahsValue;

  return jsn.dump();
}


// Deserializes object form json 
Person Deserialize(std::string jsn)
{
  json ds = json::parse(jsn);
  return Person(ds["Name"], ds["StreetNum"].get<int>(), ds["Balance"].get<double>(), ds["HashValue"]);
}


// Check if object is "empty"
bool Person::isNull()
{
    return Name == "";
}


// Clones person object by value
void Person::Clone(Person person)
{
  Name = person.Name;
  StreetNum = person.StreetNum;
  Balance = person.Balance;
  HahsValue = person.HahsValue;
}


// Table header for pretty printing
std::string Person::InfoHeader()
{
    char buff[512];
    snprintf(buff,512, "| %-20s | %-13s | %-10s | %-128s |\n", "Name", "Street Number", "Balance", "SHA-512 Hash");
    std::string strBuff(buff);
    strBuff = strBuff + std::string(strlen(buff), '-') + "\n";
    return strBuff ;
}


// Output stream operator overload
std::ostream& operator<<(std::ostream& os, const Person& person)
{
    char buff[512];
    snprintf(buff,512, "| %-20s | %-13d | %-10.2f | %s |\n", person.Name.c_str(), person.StreetNum, person.Balance, person.HahsValue.c_str());
    std::string strBuff(buff);
    os << strBuff;
    return os;
}