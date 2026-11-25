// Preview tool for the Acemagic S1 panel; see preview_app.hpp for the options.
#include "preview_app.hpp"

int main(int argc, char* argv[]) {
    PreviewDevice s1{"oos-acemagic-s1", DisplayProfile(), {}, nullptr};
    return runPreview(argc, argv, s1);
}
