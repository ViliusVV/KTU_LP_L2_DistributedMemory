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

        void Person::Clone(Person person);
        std::string Person::InfoHeader();
        bool isNull();

        Person::Person();
        Person::Person(std::string name,int streetnum, double balance);
        

        friend std::ostream& operator<<(std::ostream& os, const Person& person);
        
};

std::ostream& operator<<(std::ostream& os, const Person& person);


#endif