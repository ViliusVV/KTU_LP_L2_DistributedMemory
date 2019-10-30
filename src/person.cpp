#include "person.hpp"
#include <stdio.h>
#include <string.h>

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

bool Person::isNull()
{
    return Name == "";
}

void Person::Clone(Person person)
{
  Name = person.Name;
  StreetNum = person.StreetNum;
  Balance = person.Balance;
  HahsValue = person.HahsValue;
}

std::string Person::InfoHeader()
{
    char buff[512];
    snprintf(buff,512, "| %-20s | %-13s | %-10s | %-128s |\n", "Name", "Street Number", "Balance", "SHA-512 Hash");
    std::string strBuff(buff);
    strBuff = strBuff + std::string(strlen(buff), '-') + "\n";
    return strBuff ;
}

std::ostream& operator<<(std::ostream& os, const Person& person)
{
    char buff[512];
    snprintf(buff,512, "| %-20s | %-13d | %-10.2f | %s |\n", person.Name.c_str(), person.StreetNum, person.Balance, person.HahsValue.c_str());
    std::string strBuff(buff);
    os << strBuff;
    return os;
}