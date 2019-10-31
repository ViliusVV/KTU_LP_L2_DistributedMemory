// A2DD.h
#ifndef PERSON_H
#define PERSON_H

#include <string>

class Person
{
    public:
        std::string Name;
        int StreetNum;
        double Balance;
        std::string HahsValue;

        Person::Person();
        Person::Person(std::string name,int streetnum, double balance);
        Person::Person(std::string name,int streetnum, double balance, std::string hash);
        Person::Person(std::string jsonStr);
        void Person::Clone(Person person);
        std::string Serialize();
        Person Deserialize(std::string jsn);
        bool isNull();

        std::string Person::InfoHeader();
        

        friend std::ostream& operator<<(std::ostream& os, const Person& person);
        
};

std::ostream& operator<<(std::ostream& os, const Person& person);


#endif