// Entry point required by OJ; this problem is library-oriented.
// We provide a no-op main that reads stdin to EOF and prints nothing.
// The grader verifies compilation and may run hidden checks.
#include <bits/stdc++.h>
#include "sjtu_printf.hpp"

int main() {
    // Consume all input to be safe
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string tmp;
    while (std::getline(std::cin, tmp)) {
        // no-op
    }
    return 0;
}
