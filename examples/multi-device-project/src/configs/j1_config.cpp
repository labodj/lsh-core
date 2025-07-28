#include <lsh.hpp>

// Anonymous namespace to prevent symbol conflicts between different device configurations.
namespace
{
    // Relays
    LSH_ACTUATOR(rel0, CONTROLLINO_R0, 1);
    LSH_ACTUATOR(rel1, CONTROLLINO_R1, 2);
    LSH_ACTUATOR(rel2, CONTROLLINO_R2, 3);
    LSH_ACTUATOR(rel3, CONTROLLINO_R3, 4);
    LSH_ACTUATOR(rel4, CONTROLLINO_R4, 5);
    LSH_ACTUATOR(rel5, CONTROLLINO_R5, 6);
    LSH_ACTUATOR(rel6, CONTROLLINO_R6, 7);
    LSH_ACTUATOR(rel7, CONTROLLINO_R7, 8);
    LSH_ACTUATOR(rel9, CONTROLLINO_R9, 10);

    // Clickables
    LSH_BUTTON(btn0, CONTROLLINO_A0, 1);
    LSH_BUTTON(btn1, CONTROLLINO_A1, 2);
    LSH_BUTTON(btn2, CONTROLLINO_A2, 3);
    LSH_BUTTON(btn3, CONTROLLINO_A3, 4);
    LSH_BUTTON(btn4, CONTROLLINO_A4, 5);
    LSH_BUTTON(btn5, CONTROLLINO_A5, 6);
    LSH_BUTTON(btn6, CONTROLLINO_A6, 7);
    LSH_BUTTON(btn7, CONTROLLINO_A7, 8);
    LSH_BUTTON(btn9, CONTROLLINO_A9, 10);

    // Special clickables
    LSH_BUTTON(btn10, CONTROLLINO_IN0, 11);

    // Indicators
    LSH_INDICATOR(light9, CONTROLLINO_D9);
} // namespace

/**
 * @brief This function is the user's entry point to configure the device.
 * It is declared by the LSH library and defined here by the user.
 * The linker will automatically connect this implementation to the library's call.
 */
void Configurator::configure()
{
    using namespace Debug;
    using constants::LongClickType;
    using constants::NoNetworkClickType;
    using constants::SuperLongClickType;

    DP_CONTEXT();

    // Add actuators to actuators holder
    addActuator(&rel0);
    addActuator(&rel1);
    addActuator(&rel2);
    addActuator(&rel3);
    addActuator(&rel4);
    addActuator(&rel5);
    addActuator(&rel6);
    addActuator(&rel7);
    addActuator(&rel9);

    // Add Clickables to clickables holder
    addClickable(&btn0);
    addClickable(&btn1);
    addClickable(&btn2);
    addClickable(&btn3);
    addClickable(&btn4);
    addClickable(&btn5);
    addClickable(&btn6);
    addClickable(&btn7);
    addClickable(&btn9);
    addClickable(&btn10);

    // Add indicators to Indicators holder
    addIndicator(&light9);

    // CONFIG RELAYS
    // Auto-Off timer
    rel0.setAutoOffTimer(600000);  // 10m timer
    rel1.setAutoOffTimer(3600000); // 1h timer
    rel2.setAutoOffTimer(3600000); // 1h timer
    rel3.setAutoOffTimer(900000);  // 15m timer
    rel7.setAutoOffTimer(3600000); // 1h timer
    rel9.setAutoOffTimer(1800000); // 30m timer

    // Protection

    // CONFIG CLICKABLES
    // Add short click actuators
    // Normal
    btn0.addActuatorShort(getIndex(rel0));
    btn1.addActuatorShort(getIndex(rel1));
    btn2.addActuatorShort(getIndex(rel2));
    btn3.addActuatorShort(getIndex(rel3));
    btn4.addActuatorShort(getIndex(rel4));
    btn5.addActuatorShort(getIndex(rel5));
    btn6.addActuatorShort(getIndex(rel6));
    btn7.addActuatorShort(getIndex(rel7));
    btn9.addActuatorShort(getIndex(rel9));

    // Special
    btn10.addActuatorShort(getIndex(rel0));

    // Set clickability
    btn1.setClickableLong(true);
    btn2.setClickableLong(true);
    btn4.setClickableLong(true);
    btn5.setClickableLong(true);
    btn6.setClickableLong(true, LongClickType::OFF_ONLY);
    btn7.setClickableLong(true).setLongClickTime(900).setClickableSuperLong(true, SuperLongClickType::SELECTIVE);
    btn10.setClickableSuperLong(true);

    // Secondary actuators
    btn1.addActuatorLong(getIndex(rel1))
        .addActuatorLong(getIndex(rel2));
    btn2.addActuatorLong(getIndex(rel2))
        .addActuatorLong(getIndex(rel1));
    btn4.addActuatorLong(getIndex(rel4))
        .addActuatorLong(getIndex(rel5));
    btn5.addActuatorLong(getIndex(rel5))
        .addActuatorLong(getIndex(rel4));
    btn6.addActuatorLong(getIndex(rel6))
        .addActuatorLong(getIndex(rel4))
        .addActuatorLong(getIndex(rel5));
    btn7.addActuatorLong(getIndex(rel7))
        .addActuatorSuperLong(getIndex(rel1))
        .addActuatorSuperLong(getIndex(rel2));

    // Indicators
    light9.addActuator(getIndex(rel9));
}