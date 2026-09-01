#pragma once

#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;
struct _lv_event_t;
typedef struct _lv_event_t lv_event_t;

namespace coronet {

class SetupWizard {
public:
    void begin();
    void loop();
    void reset();
    bool finished() const { return finished_; }
    bool active() const { return root_ != nullptr && !finished_; }

private:
    enum class Step : uint8_t {
        Welcome = 0,
        Identity,
        Connection,
        Network,
        Printer,
        Ready,
        Count,
    };

    enum class Action : uintptr_t {
        Back = 1,
        Next,
        TransportAuto = 10,
        TransportBle,
        TransportWifi,
        NetworkRefresh = 20,
        NetworkManual,
        NetworkChange,
        NetworkBase = 100,
        PrinterRefresh = 30,
        PrinterManual,
        PrinterChange,
        PrinterBase = 200,
    };

    void buildRoot();
    void renderStep();
    void renderWelcome();
    void renderIdentity();
    void renderConnection();
    void renderNetwork();
    void renderNetworkDiscovery();
    void renderNetworkCredentials();
    void renderPrinter();
    void renderPrinterDiscovery();
    void renderPrinterDetails();
    void renderReady();
    void commitCurrentStep();
    void moveNext();
    void moveBack();
    void finish();
    void showKeyboard(lv_obj_t* textarea);
    void hideKeyboard();
    void updateNavigation();
    void updateProgress();

    static void actionEvent(lv_event_t* event);
    static void fieldEvent(lv_event_t* event);
    static void keyboardEvent(lv_event_t* event);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* stepLabel_ = nullptr;
    lv_obj_t* backButton_ = nullptr;
    lv_obj_t* nextButton_ = nullptr;
    lv_obj_t* nextButtonLabel_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* nameField_ = nullptr;
    lv_obj_t* ssidField_ = nullptr;
    lv_obj_t* passwordField_ = nullptr;
    lv_obj_t* printerHostField_ = nullptr;
    lv_obj_t* printerPortField_ = nullptr;
    lv_obj_t* progress_[static_cast<uint8_t>(Step::Count)] = {};
    Step step_ = Step::Welcome;
    bool networkCredentialsView_ = false;
    bool selectedNetworkSecured_ = true;
    bool networkConnectionVerified_ = false;
    bool printerDetailsView_ = false;
    bool renderPending_ = false;
    bool finished_ = false;
    uint32_t wifiScanRevisionSeen_ = 0;
    uint32_t wifiConnectionRevisionSeen_ = 0;
    uint32_t printerDiscoveryRevisionSeen_ = 0;
    char selectedSsid_[33] = "";
    char networkPassword_[65] = "";
    char selectedPrinterName_[49] = "";
    char selectedPrinterHost_[65] = "";
    uint16_t selectedPrinterPort_ = 7125;
};

}
