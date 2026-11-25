// Preview tool for the Acemagic S1 panel; see preview_app.hpp for the options.
#include "preview_app.hpp"

int main(int argc, char* argv[]) {
    // Every page the S1 can show is rendered and checked, including the optional
    // MQTT page that is off by default
    PreviewDevice s1{"oos-acemagic-s1", DisplayProfile(),
                     {"system", "network", "sensors", "docker", "services", "identity", "mqtt"}, {}, nullptr};
    return runPreview(argc, argv, s1);
}
