#ifndef UTIL_UUID_H
#define UTIL_UUID_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "singleton.h"

class UUID : public Singleton<UUID> 
{
public:
    //UUID() : generator(std::make_unique<boost::uuids::random_generator>()) {std::cout << "UUID constructor" << std::endl;}
    static boost::uuids::uuid GenerateUUID() 
    {
        return GetInstance().generator->operator()();
    }

    static std::string GenerateUUIDAsString()
    {
        return boost::uuids::to_string(GenerateUUID());
    }
protected:
    std::unique_ptr<boost::uuids::random_generator> generator;
};

#endif // UTIL_UUID_H