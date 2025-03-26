// INsfwScanner.h
#ifndef INsfwScanner_H
#define INsfwScanner_H
#include <string>

class INsfwScanner {
public:
    virtual bool scan(const std::string& filePath, float threshold) = 0;
    virtual ~INsfwScanner() = default;
};

#endif
