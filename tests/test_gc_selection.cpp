
#include "ssd/ftl.hpp"
#include <iostream>
#include <stdexcept>

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("GC selection test failed");
    }
}

int main() {
    // Case 1: 沒有 Invalid Page
    {
        ssd::FTL ftl{2, 3};
        require(ftl.write(10));
        require(!ftl.selectVictimBlock().has_value());
    }

    // Case 2: Block 0 可被選為 Victim
    {
        ssd::FTL ftl{3, 3};

        require(ftl.write(10));
        require(ftl.write(20));
        require(ftl.write(30));
        require(ftl.write(10));

        auto victim = ftl.selectVictimBlock();

        require(victim.has_value());
        require(*victim == 0);
    }

    // Case 3: 沒有足夠空間搬移 Valid Pages
    {
        ssd::FTL ftl{2, 2};

        require(ftl.write(10));
        require(ftl.write(20));
        require(ftl.write(10));
        require(ftl.write(30));

        require(!ftl.selectVictimBlock().has_value());
    }

    // Case 4: 優先選擇 Invalid Pages 最多的 Block
    {
        ssd::FTL ftl{4, 3};

        require(ftl.write(10));
        require(ftl.write(11));
        require(ftl.write(12));

        require(ftl.write(20));
        require(ftl.write(21));
        require(ftl.write(22));

        require(ftl.write(10));
        require(ftl.write(11));
        require(ftl.write(20));

        auto victim = ftl.selectVictimBlock();

        require(victim.has_value());
        require(*victim == 0);
    }

    std::cout << "[PASS] GC victim selection\n";
}
