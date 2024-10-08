#include <iostream>
#include "../src/log.h"
#include "../src/config.h"
sylar::ConfigVar<int>::ptr g_int_value_config = sylar::Config::Lookup("system.port", (int)8080, "system port");
sylar::ConfigVar<float>::ptr g_int_xvalue_config = sylar::Config::Lookup("system.port_float", (float)8080, "system port");
sylar::ConfigVar<std::vector<int>>::ptr g_int_vector_value_config = sylar::Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");
sylar::ConfigVar<std::list<int>>::ptr g_int_list_value_config = sylar::Config::Lookup("system.int_list", std::list<int>{1,2}, "system int list");
sylar::ConfigVar<std::set<int>>::ptr g_int_set_value_config = sylar::Config::Lookup("system.int_set", std::set<int>{6,8}, "system int set");
sylar::ConfigVar<std::unordered_set<int>>::ptr g_int_unordered_set_value_config = sylar::Config::Lookup("system.int_unordered_set", std::unordered_set<int>{7,77,777}, "system int unordered set");
sylar::ConfigVar<std::map<std::string, int>>::ptr g_int_map_value_config = sylar::Config::Lookup("system.int_map", std::map<std::string, int>{{"7",7},{"777",777},{"77",77}}, "system int map");
sylar::ConfigVar<std::unordered_map<std::string, int>>::ptr g_int_unordered_map_value_config = sylar::Config::Lookup("system.int_unordered_map", std::unordered_map<std::string, int>{{"7",7},{"777",777},{"77",77}}, "system int map");

void print_yaml(const YAML::Node& node, int level) {
    if(node.IsScalar()){
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level*4, ' ') << node.Scalar() << "-" << node.Type() << " - " << level;;
    } else if(node.IsNull()){
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level*4, ' ') <<  "NULL - " << node.Type() << " - " << level;
    } else if(node.IsMap()){
        for(auto it = node.begin();it != node.end();it++){
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level*4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    } else if(node.IsSequence()){
        for(size_t i = 0; i<node.size(); i++){
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level*4, ' ') <<  i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}

void test_yaml(){
    YAML::Node root = YAML::LoadFile("../src/config.yaml");
    print_yaml(root, 0);

    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << root;
}

void test_config() {
#define XX(g_var, name, prefix)  \
    { \
    auto v = g_var->getValue(); \
    for(auto i : v){ \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix << " : " << #name << " : " << i;\
    } \
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix << " : " << #name << " yaml : " << g_var->toString(); \
    } \

#define XX_MAP(g_var, name, prefix)  \
    { \
    auto v = g_var->getValue(); \
    for(auto i : v){ \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix << " : " << #name << " : {" << i.first << " - " << i.second << "}";\
    } \
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix << " : " << #name << " yaml : " << g_var->toString(); \
    } \

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before : " << g_int_value_config->getValue();
    XX(g_int_vector_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_unordered_set_value_config, int_unordered_set, before);
    XX_MAP(g_int_map_value_config, int_map, before);
    XX_MAP(g_int_unordered_map_value_config, int_unordered_map, before);

    YAML::Node root = YAML::LoadFile("../src/config.yaml");
    sylar::Config::LoadFromYaml(root);

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after  : " << g_int_value_config->getValue();
    XX(g_int_vector_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_list, after);
    XX(g_int_unordered_set_value_config, int_unordered_set, after);
    XX_MAP(g_int_map_value_config, int_map, after);
    XX_MAP(g_int_unordered_map_value_config, int_unordered_map, after);
}

class Person{
public:
    std::string m_name = "zjy";
    int m_age = 0;
    bool m_sex = 0;

    Person(){}

    bool operator==(const Person& oth) const {
        return m_name == oth.m_name && m_age == oth.m_age && m_sex == oth.m_sex;
    }

    Person& operator=(const Person& other) {  
        if (this != &other) {  
            m_name = other.m_name;  
            m_age = other.m_age;  
            m_sex = other.m_sex;  
        }  
        return *this;  
    }  

    Person(const Person &t){
        this->m_age = t.m_age;
        this->m_name= t.m_name;
        this->m_sex = t.m_sex;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name =" << m_name
           << " age = " << m_age
           << " sex = " << m_sex << "]";
        return ss.str();
    }
};


namespace sylar{
template<>
class LexicalCast<std::string, Person>{
public:
    Person operator()(const std::string &v ){
        YAML::Node node = YAML::Load(v);
        Person p;
        std::stringstream ss;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};


template<>
class LexicalCast<Person, std::string>{
public:
    std::string operator()(const Person &p) {
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
}

sylar::ConfigVar<Person>::ptr g_person = sylar::Config::Lookup("class.person", Person(), "system person");

void test_class(){
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before : " << g_person->getValue().toString() << " - " << g_person->toString();

    YAML::Node root = YAML::LoadFile("../src/config.yaml");
    sylar::Config::LoadFromYaml(root);

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after  : " << g_person->getValue().toString() << " - " << g_person->toString();

    g_person->addListener([](const Person& old_value, const Person& new_value){
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "old_value = " << old_value.toString() << "new_value = " << new_value.toString();
    });

}

void test_log(){
    static sylar::Logger::ptr system_log = SYLAR_LOG_NAME("system");
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("../src/config.yaml");
    sylar::Config::LoadFromYaml(root);
    std::cout << "====================================================" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << root << std::endl;
    std::cout << "====================================================" << std::endl;
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;

    system_log->setFormatter("%d - %m%n");
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;


    std::cout << "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" << std::endl;
    sylar::Config::Visit([](sylar::ConfigVarBase::ptr var){
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "name=" << var->getName()
                                 << " description=" << var->getDescription()
                                 << " typename=" << var->getTypeName()
                                 << " value=" << var->toString();
    });

    // SYLAR_LOG_INFO(system_log) << "123123123";
    // SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "999999999999999999999";
    // SYLAR_LOG_INFO(system_log) << SYLAR_LOG_ROOT()->m_appender.front()->toYamlString();
    // SYLAR_LOG_INFO(system_log) << SYLAR_LOG_ROOT()->m_appender.back()->toYamlString();
}

int main(int argc, char** argv) {
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << g_int_value_config->getValue();
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << g_int_value_config->toString();
    // test_yaml();
    // test_config();
    //test_class();
    test_log();
    
    return 0;
}