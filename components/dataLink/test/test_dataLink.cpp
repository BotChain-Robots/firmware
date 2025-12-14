#include "unity.h"
#include "DataLinkManager.h"
#include <memory>

#define TEST_BOARD_ID 69

std::unique_ptr<DataLinkManager> createObj(){
    std::unique_ptr<DataLinkManager> obj = std::make_unique<DataLinkManager>(TEST_BOARD_ID, 4);
    TEST_ASSERT_NOT_NULL(obj.get());
    return obj;
}

TEST_CASE("should instantiate an DataLinkManager object with 4 channels", "[dataLink]"){    
    createObj();
}

// void board_a(){
//     std::unique_ptr<DataLinkManager> obj = createObj();
//     unity_send_signal("board a");
//     unity_wait_for_signal("board b");
// }

// void board_b(){
//     std::unique_ptr<DataLinkManager> obj = createObj();
//     unity_send_signal("board b");
//     unity_wait_for_signal("board a");
// }

// TEST_CASE_MULTIPLE_DEVICES("should be able to send tables to another board and receive it", "[dataLink]", board_a, board_b);