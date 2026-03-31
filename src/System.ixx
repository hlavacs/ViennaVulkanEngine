export module VEEngine:System;
import std;

export namespace vve {

class System {
public:
    virtual ~System() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

} // namespace vve
