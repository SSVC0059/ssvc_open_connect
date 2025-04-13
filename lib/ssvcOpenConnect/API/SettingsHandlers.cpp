//
// Created by demoncat on 23.03.2025.
//

#include "SettingsHandlers.h"
#include "core/SsvcSettings.h"

std::map<String, std::unique_ptr<ParamHandler>> createHandlers() {
  std::map<String, std::unique_ptr<ParamHandler>> handlers;

  // Hyst
  handlers.emplace(
      "hyst",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, float v) { b.setHysteresis(v); })));

  // Heads
  handlers.emplace(
      "heads",
      std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
          [](SsvcSettings::Builder &b, float t, int p) { b.setHeads(t, p); })));

  handlers.emplace("late_heads",
                   std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
                       [](SsvcSettings::Builder &b, float t, int p) {
                         b.setLateHeads(t, p);
                       })));

  // Hearts
  handlers.emplace("hearts",
                   std::unique_ptr<TwoValueHandler>(
                       new TwoValueHandler([](SsvcSettings::Builder &b, float t,
                                              int p) { b.setHearts(t, p); })));

  handlers.emplace(
      "tails",
      std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
          [](SsvcSettings::Builder &b, float t, int p) { b.setTails(t, p); })));

  // Decrement
  handlers.emplace("decrement",
                   std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                       [](SsvcSettings::Builder &b, unsigned char v) {
                         b.setDecrement(v);
                       })));

  // formula
  handlers.emplace(
      "formula",
      std::unique_ptr<BooleanHandler>(new BooleanHandler(
          [](SsvcSettings::Builder &b, bool v) { b.formulaEnable(v); })));

  // tank_mmhg
  handlers.emplace("tank_mmhg",
                   std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                       [](SsvcSettings::Builder &b, unsigned char v) {
                         b.setTank_mmhg(v);
                       })));

  // heads_timer
  handlers.emplace("heads_timer",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, unsigned int v) {
                         b.setHeadsTimer(v);
                       })));

  // late_heads_timer
  handlers.emplace("late_heads_timer",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, unsigned int v) {
                         b.setLateHeadsTimer(v);
                       })));

  // hearts_timer
  handlers.emplace("hearts_timer",
                   std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                       [](SsvcSettings::Builder &b, unsigned char v) {
                         b.setHeartsTimer(v);
                       })));

  // tails_temp
  handlers.emplace(
      "tails_temp",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, float v) { b.setTailsTemp(v); })));

  // start_delay
  handlers.emplace("start_delay",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, unsigned int v) {
                         b.setStartDelay(v);
                       })));

  // hearts_finish_temp
  handlers.emplace("hearts_finish_temp",
                   std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
                       [](SsvcSettings::Builder &b, float v) {
                         b.setHeartsFinishTemp(v);
                       })));

  // formula_start_temp
  handlers.emplace("formula_start_temp",
                   std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
                       [](SsvcSettings::Builder &b, float v) {
                         b.setFormulaStartTemp(v);
                       })));

  // setValveBw
  handlers.emplace("valve_bw",
                   std::unique_ptr<ThreeIntHandler>(new ThreeIntHandler(
                       [](SsvcSettings::Builder &b, int v1, int v2, int v3) {
                         b.setValveBw(v1, v2, v3);
                       })));

  // release_speed
  handlers.emplace(
      "release_speed",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, float v) { b.setReleaseSpeed(v); })));

  // release_speed
  handlers.emplace("release_timer",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, unsigned int v) {
                         b.setReleaseTimer(v);
                       })));

  // heads_final
  handlers.emplace(
      "heads_final",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, float v) { b.setHeadsFinal(v); })));

  return handlers;
}

const std::map<String, std::unique_ptr<ParamHandler>> PARAM_HANDLERS =
    createHandlers();