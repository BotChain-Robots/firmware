//
// Created by Johnathon Slightham on 2025-05-26.
//

#ifndef LOGGER_H
#define LOGGER_H

class Logger {
public:
    static Logger &getInstance() {
        static Logger instance;
        return instance;
    }

private:
    virtual Logger();

}

#endif //LOGGER_H
