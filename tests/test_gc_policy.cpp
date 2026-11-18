
#include "ssd/ftl.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "GC Policy test failed"
        );
    }
}


void testClosedEpochReuse() {
    ssd::Flash flash{2, 2};

    auto a = flash.program(10);
    check(a.has_value());

    // Block 0 還沒寫滿
    check(!flash.closedEpoch(0).has_value());

    auto b = flash.program(11);
    check(b.has_value());

    // Block 0 剛寫滿
    auto epoch0 = flash.closedEpoch(0);
    check(epoch0.has_value());

    auto c = flash.program(20);
    check(c.has_value());

    auto d = flash.program(21);
    check(d.has_value());

    // Block 1 比 Block 0 晚寫滿
    auto epoch1 = flash.closedEpoch(1);
    check(epoch1.has_value());
    check(*epoch0 < *epoch1);

    // 清除 Block 0
    check(flash.invalidate(*a));
    check(flash.invalidate(*b));
    check(flash.eraseBlock(0));

    check(!flash.closedEpoch(0).has_value());

    // 重新使用 Block 0
    check(flash.program(30).has_value());
    check(!flash.closedEpoch(0).has_value());

    check(flash.program(31).has_value());

    auto new_epoch0 = flash.closedEpoch(0);

    check(new_epoch0.has_value());

    // 重新寫滿後，Block 0 應該比 Block 1 新
    check(*new_epoch0 > *epoch1);
}


int main() {

    ssd::FTL greedy{
        4, 3, ssd::GcPolicy::Greedy
    };

    ssd::FTL fifo{
        4, 3, ssd::GcPolicy::FIFO
    };

    constexpr std::array<ssd::LogicalPageId, 9> writes{
        10, 11, 12,
        20, 21, 22,
        10, 20, 21
    };

    for (auto lpn : writes) {
        check(greedy.write(lpn));
        check(fifo.write(lpn));
    }

    // Block 0：
    // 1 Invalid、2 Valid
    //
    // Block 1：
    // 2 Invalid、1 Valid
    //
    // Block 2：
    // 3 Valid
    //
    // Block 3：
    // 3 Free

    auto greedy_victim =
        greedy.selectVictimBlock();

    auto fifo_victim =
        fifo.selectVictimBlock();

    check(greedy_victim.has_value());
    check(fifo_victim.has_value());

    // Greedy 選最多 Invalid Pages
    check(*greedy_victim == 1);

    // FIFO 選最早寫滿的 Block
    check(*fifo_victim == 0);

    // 檢查 Epoch 的順序
    auto epoch0 = fifo.flash().closedEpoch(0);
    auto epoch1 = fifo.flash().closedEpoch(1);

    check(epoch0.has_value());
    check(epoch1.has_value());
    check(*epoch0 < *epoch1);
    testClosedEpochReuse();

    std::cout
        << "[PASS] Greedy vs FIFO selection\n";
}
