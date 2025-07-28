#include <lsh.hpp>

// Anonymous namespace to prevent symbol conflicts between different device configurations.
namespace
{
    // Relays
    LSH_ACTUATOR(rel0, CONTROLLINO_R0, 1);
    LSH_ACTUATOR(rel1, CONTROLLINO_R1, 2);
    LSH_ACTUATOR(rel2, CONTROLLINO_R2, 3);
    LSH_ACTUATOR(rel3, CONTROLLINO_R3, 4);
    LSH_ACTUATOR(rel6, CONTROLLINO_R6, 7);
    LSH_ACTUATOR(rel7, CONTROLLINO_R7, 8);
    LSH_ACTUATOR(rel8, CONTROLLINO_R8, 9);
    LSH_ACTUATOR(rel9, CONTROLLINO_R9, 10);

    // Clickables
    LSH_BUTTON(btn0, CONTROLLINO_A0, 1);
    LSH_BUTTON(btn1, CONTROLLINO_A1, 2);
    LSH_BUTTON(btn2, CONTROLLINO_A2, 3);
    LSH_BUTTON(btn3, CONTROLLINO_A3, 4);
    LSH_BUTTON(btn6, CONTROLLINO_A6, 7);
    LSH_BUTTON(btn7, CONTROLLINO_A7, 8);
    LSH_BUTTON(btn8, CONTROLLINO_A8, 9);
    LSH_BUTTON(btn9, CONTROLLINO_A9, 10);

    // Special clickables
    LSH_BUTTON(btn10, CONTROLLINO_IN0, 11);
    LSH_BUTTON(btn11, CONTROLLINO_IN1, 12);

    // Indicators
    LSH_INDICATOR(light6, CONTROLLINO_D6);
    LSH_INDICATOR(light7, CONTROLLINO_D7);
    LSH_INDICATOR(light8, CONTROLLINO_D8);
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
    addActuator(&rel6);
    addActuator(&rel7);
    addActuator(&rel8);
    addActuator(&rel9);

    // Add Clickables to clickables holder
    addClickable(&btn0);
    addClickable(&btn1);
    addClickable(&btn2);
    addClickable(&btn3);
    addClickable(&btn6);
    addClickable(&btn7);
    addClickable(&btn8);
    addClickable(&btn9);
    addClickable(&btn10);
    addClickable(&btn11);

    // Add indicators to Indicators holder
    addIndicator(&light6);
    addIndicator(&light7);
    addIndicator(&light8);

    // CONFIG RELAYS
    // Auto-Off timer
    rel7.setAutoOffTimer(3600000); // 1h timer
    rel8.setAutoOffTimer(1800000); // 30m timer

    // Protection
    rel6.setProtected(true);

    // CONFIG CLICKABLES
    // Add short click actuators
    // Normal
    btn0.addActuatorShort(getIndex(rel0));
    btn1.addActuatorShort(getIndex(rel1));
    btn2.addActuatorShort(getIndex(rel2));
    btn3.addActuatorShort(getIndex(rel3));
    btn6.addActuatorShort(getIndex(rel6));
    btn7.addActuatorShort(getIndex(rel7));
    btn8.addActuatorShort(getIndex(rel8));
    btn9.addActuatorShort(getIndex(rel9));

    // Special
    btn10.addActuatorShort(getIndex(rel0));
    btn11.addActuatorShort(getIndex(rel2));

    // Set clickability
    btn0.setClickableLong(true);
    btn1.setClickableLong(true, LongClickType::NORMAL, true, NoNetworkClickType::DO_NOTHING);
    btn2.setClickableLong(true);
    btn3.setClickableLong(true);
    btn9.setClickableLong(true);
    btn10.setClickableLong(true)
        .setClickableSuperLong(true);
    btn11.setClickableLong(true)
        .setClickableSuperLong(true, SuperLongClickType::NORMAL, true, NoNetworkClickType::DO_NOTHING);

    // Secondary actuators
    btn0.addActuatorLong(getIndex(rel0))
        .addActuatorLong(getIndex(rel2));
    btn2.addActuatorLong(getIndex(rel2))
        .addActuatorLong(getIndex(rel1));
    btn3.addActuatorLong(getIndex(rel3))
        .addActuatorLong(getIndex(rel9));
    btn9.addActuatorLong(getIndex(rel9))
        .addActuatorLong(getIndex(rel3));
    btn10.addActuatorLong(getIndex(rel0))
        .addActuatorLong(getIndex(rel2));
    btn11.addActuatorLong(getIndex(rel2))
        .addActuatorLong(getIndex(rel1));

    // Indicators
    light6.addActuator(getIndex(rel6));
    light7.addActuator(getIndex(rel7));
    light8.addActuator(getIndex(rel8));
}