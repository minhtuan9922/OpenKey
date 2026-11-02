#include "Composition.h"
#include <iostream>
#include <stdexcept>

void require(bool value) {
    if (!value) throw std::runtime_error("Composition assertion failed");
}
void type(openkey::Composition& value, const std::string& input) {
    for (char ch : input) require(value.append(ch));
}
int main() {
    openkey::Composition telex;
    type(telex, "tieengs");
    require(telex.text() == "tiếng");
    require(telex.backspace());
    require(telex.text() == "tiến");
    type(telex, "g");
    require(telex.text() == "tiếng");

    openkey::Composition vni(vVNI);
    type(vni, "vie6t5");
    require(vni.text() == "việt");
    // Interleave contexts/modes: replay must not leak state across editors.
    type(telex, "s");
    require(vni.text() == "việt");
    require(vni.backspace());
    require(vni.text() == "việ");
    require(!vni.append(' '));
    vni.clear();
    require(vni.empty() && !vni.backspace());

    openkey::Composition capitals;
    type(capitals, "TIEENGS");
    require(capitals.text() == "TIẾNG");
    capitals.clear();
    type(capitals, "hoasn");
    require(capitals.text() == "hoán");
    require(capitals.backspace());
    require(capitals.text() == "hoá");
    capitals.clear();
    for (std::size_t i = 0; i < openkey::Composition::MaxEvents; ++i)
        require(capitals.append('b'));
    require(capitals.full() && !capitals.append('b'));
    std::cout << "Composition: Telex/VNI, Backspace, case, context isolation and limit passed\n";
}
