/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include "SettingsSetterHandlers.h"

std::map<String, std::unique_ptr<ParamHandler>> createHandlers() {
  std::map<String, std::unique_ptr<ParamHandler>> handlers;

  // Hyst
  handlers.emplace(
      "hyst",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, const float v) { b.setHysteresis(v); })));

  // Heads
  handlers.emplace(
      "heads",
      std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
          [](SsvcSettings::Builder &b, const float t, const int p) { b.setHeads(t, p); })));

  handlers.emplace("late_heads",
                   std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
                       [](SsvcSettings::Builder &b, const float t, const int p) {
                         b.setLateHeads(t, p);
                       })));

  // Hearts
  handlers.emplace("hearts",
                   std::unique_ptr<TwoValueHandler>(
                       new TwoValueHandler([](SsvcSettings::Builder &b, const float t,
                                              const int p) { b.setHearts(t, p); })));

  handlers.emplace(
      "tails",
      std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
          [](SsvcSettings::Builder &b, const float t, int p) { b.setTails(t, p); })));

  // Decrement
  handlers.emplace("decrement",
                   std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                       [](SsvcSettings::Builder &b, const unsigned char v) {
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
                       [](SsvcSettings::Builder &b, const unsigned char v) {
                         b.setTank_mmhg(v);
                       })));

  // heads_timer
  handlers.emplace("heads_timer",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, const unsigned int v) {
                         b.setHeadsTimer(v);
                       })));

  // late_heads_timer
  handlers.emplace("late_heads_timer",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, const unsigned int v) {
                         b.setLateHeadsTimer(v);
                       })));

  // hearts_timer
  handlers.emplace("hearts_timer",
                   std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                       [](SsvcSettings::Builder &b, const unsigned char v) {
                         b.setHeartsTimer(v);
                       })));

  // tails_temp
  handlers.emplace(
      "tails_temp",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, const float v) { b.setTailsTemp(v); })));

  // start_delay
  handlers.emplace("start_delay",
                   std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                       [](SsvcSettings::Builder &b, const unsigned int v) {
                         b.setStartDelay(v);
                       })));

  // hearts_finish_temp
  handlers.emplace("hearts_finish_temp",
                   std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
                       [](SsvcSettings::Builder &b, const float v) {
                         b.setHeartsFinishTemp(v);
                       })));

  // formula_start_temp
  handlers.emplace("formula_start_temp",
                   std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
                       [](SsvcSettings::Builder &b, const float v) {
                         b.setFormulaStartTemp(v);
                       })));

  // setValveBw
  handlers.emplace("valve_bw",
                   std::unique_ptr<ThreeIntHandler>(new ThreeIntHandler(
                       [](SsvcSettings::Builder &b, const int v1, const int v2, const int v3) {
                         b.setValveBw(v1, v2, v3);
                       })));

  // release_speed
  handlers.emplace(
      "release_speed",
      std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, const float v) { b.setReleaseSpeed(v); })));

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
          [](SsvcSettings::Builder &b, const float v) { b.setHeadsFinal(v); })));

    // Heads
  handlers.emplace(
        "parallel",
        std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
            [](SsvcSettings::Builder &b, const float t, const int p) { b.setParallel(t, p); })));

  // parallel_v1
  handlers.emplace(
      "parallel_v1",
      std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
          [](SsvcSettings::Builder &b, const float t, const int p) { b.setParallelV1(t, p); })));


  // parallel_v3
  handlers.emplace(
      "parallel_v3",
      std::unique_ptr<ArrayOfFloatFloatIntHandler>(
          new ArrayOfFloatFloatIntHandler([](SsvcSettings::Builder &b,
                                             const std::vector<ArrayOfFloatFloatIntHandler::Values> &v) {
            std::vector<std::tuple<float, float, int>> values;
            values.reserve(v.size());
            for (const auto& item : v) {
                values.emplace_back(item.f1, item.f2, item.i1);
            }
            b.setParallelV3(values);
          })));

// хеддеры для оперативных настроек
    handlers.emplace(
        "s_temp",
        std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
          [](SsvcSettings::Builder &b, const float v) { b.setStepTemp(v); }))
        );

    handlers.emplace(
        "s_hyst",
        std::unique_ptr<SingleFloatHandler>(new SingleFloatHandler(
            [](SsvcSettings::Builder &b, const float v)
            {
                b.setStepHysteresis(v);
            })));

    handlers.emplace(
    "s_speed",
    std::unique_ptr<TwoValueHandler>(new TwoValueHandler(
        [](SsvcSettings::Builder &b, const float t, const int p)
        {
            b.setStepSpeed(t, p);
        })));

    handlers.emplace(
        "s_decrement",
                 std::unique_ptr<UnsignedCharHandler>(new UnsignedCharHandler(
                     [](SsvcSettings::Builder &b, const unsigned char v) {
                       b.setStepDecrement(v);
                     })));

    handlers.emplace(
        "s_timer",
                 std::unique_ptr<UnsignedIntHandler>(new UnsignedIntHandler(
                     [](SsvcSettings::Builder &b,const unsigned int v) {
                       b.setStepTimer(v);
                     })));

  return handlers;
}

const std::map<String, std::unique_ptr<ParamHandler>> PARAM_HANDLERS =
    createHandlers();
