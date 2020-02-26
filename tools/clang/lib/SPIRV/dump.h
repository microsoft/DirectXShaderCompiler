#include <iostream>

#define jdump(expr) \
    do { \
      std::cout << #expr << std::endl; \
      std::cout << expr->getLocStart().printToString(astContext.getSourceManager()) << std::endl; \
      std::cout << expr->getLocEnd().printToString(astContext.getSourceManager()) << std::endl; \
      std::cout << expr->getType().getAsString() << std::endl; \
      std::cout << __FUNCTION__ << " " << __LINE__ << std::endl << std::flush; \
    } while(0)

#define jlog(msg) \
    std::cout << __FUNCTION__ << " " << __LINE__ << " " << msg << std::endl

#define tdump(type) \
    do { \
      std::cout << #type << std::endl; \
      std::cout << type.getAsString() << std::endl; \
      std::cout << __FUNCTION__ << " " << __LINE__ << std::endl << std::flush; \
    } while(0)

#define jline(ctx, loc) \
    do { \
      const auto &sm = (ctx).getSourceManager(); \
      uint32_t line = sm.getPresumedLineNumber(loc); \
      uint32_t column = sm.getPresumedColumnNumber(loc); \
      std::cout << __FUNCTION__ \
      << ", line: " << line \
      << ", column: " << column \
      << " " << __LINE__ << std::endl << std::flush; \
    } while(0)
