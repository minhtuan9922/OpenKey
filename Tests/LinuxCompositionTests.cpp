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
    for (int mode : {vTelex, vVNI}) {
        for (const auto* command : {"ls", "git", "cd"}) {
            openkey::Composition shellWord(mode);
            type(shellWord, command);
            if (shellWord.text() != command)
                throw std::runtime_error("Command mode=" + std::to_string(mode) + " input=" +
                                         command + " output=" + shellWord.text());
        }
    }
    require(!openkey::Composition::Options{}.quickTelex);
    for (const auto& [input, output] : {
            std::pair{"cc", "ch"}, {"gg", "gi"}, {"kk", "kh"}, {"nn", "ng"},
            {"qq", "qu"}, {"pp", "ph"}, {"tt", "th"}}) {
        openkey::Composition quick(vTelex, {true, true, true});
        openkey::Composition normal;
        openkey::Composition numeric(vVNI, {true, true, true});
        for (char ch : std::string(input)) {
            require(quick.append(ch));
            require(normal.append(ch));
            require(numeric.append(ch));
        }
        require(quick.text() == output && normal.text() == input && numeric.text() == input);
        auto copy = quick;
        require(quick.backspace() && quick.text() == std::string(output).substr(0, 1));
        require(copy.text() == output);
        require(quick.append(input[0]) && quick.text() == output);
        copy.setOptions({true, true, false});
        require(copy.text() == input && quick.text() == output);
        copy.setOptions({true, true, true});
        require(copy.text() == output);
        quick.clear();
        std::string upperInput(input), upperOutput(output);
        for (char& ch : upperInput) ch = static_cast<char>(ch - 'a' + 'A');
        for (char& ch : upperOutput) ch = static_cast<char>(ch - 'a' + 'A');
        type(quick, upperInput);
        require(quick.text() == upperOutput);
        require(quick.backspace() && quick.text() == upperOutput.substr(0, 1));
        quick.clear();
        type(quick, std::string(input) + "a");
        require(quick.text() == std::string(output) + "a");
    }
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

    for (int mode : {vTelex, vVNI}) {
        const auto tone = mode == vTelex ? "hoas" : "hoa1";
        const auto spelling = mode == vTelex ? "fas" : "fa1";
        for (bool check : {true, false}) {
            openkey::Composition modern(mode, {check, true});
            openkey::Composition classic(mode, {check, false});
            // Alternating every event forces replay to restore both globals.
            for (char ch : std::string(tone)) {
                require(modern.append(ch));
                require(classic.append(ch));
            }
            require(modern.text() == "hoá" && classic.text() == "hóa");
            auto copy = classic;
            require(modern.backspace() && modern.text() == "ho");
            require(copy.append(mode == vTelex ? 'f' : '2'));
            require(copy.text() == "hòa" && classic.text() == "hóa");
            require(modern.append('a') && modern.append(mode == vTelex ? 's' : '1'));
            require(modern.text() == "hoá");
            classic.setOptions({check, true});
            require(classic.text() == "hoá");
            modern.clear();
            classic.clear();
            type(modern, mode == vTelex ? "thuys" : "thuy1");
            type(copy, "b"); // Copied classic options remain independent.
            require(modern.text() == "thuý");
            classic.setOptions({check, false});
            type(classic, mode == vTelex ? "thuys" : "thuy1");
            require(classic.text() == "thúy");
        }
        openkey::Composition checked(mode), unchecked(mode, {false, true});
        for (char ch : std::string(spelling)) {
            require(unchecked.append(ch));
            require(checked.append(ch));
        }
        require(checked.text() == spelling && unchecked.text() == "fá");
        require(unchecked.backspace() && unchecked.text() == "f");
        type(unchecked, mode == vTelex ? "as" : "a1");
        require(unchecked.text() == "fá");
        checked.setOptions({false, false});
        require(checked.text() == "fá");
        checked.clear();
        type(checked, spelling);
        require(checked.text() == "fá");
    }
    std::cout << "Composition: Telex/VNI, spelling, orthography, replay/copy isolation, Backspace, case and limit passed\n";
}
