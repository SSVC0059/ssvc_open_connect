<!-- Telemetry.svelte -->
<script lang="ts">
    import {Card, Popover, Label, Alert } from "flowbite-svelte";
    import { InfoCircleSolid } from 'flowbite-svelte-icons';

    import { getStageName } from "../scripts/getStageName"
    import {activeSection} from "../scripts/store.js"

    export let data;

    $: if (data) {
        // console.log("Полученные данные для отображения в Telemetry:", data); // Проверка данных для отображения
    } else {
        console.error(" Данных для отображения в Telemetry нет"); // Проверка данных для отображения
    }

    let commonIsOpen = true; // Состояние для управления спойлером
    let paramsIsOpen = true; // Состояние для управления спойлером


    // Функция для проверки наличия поля
    function isFieldPresent(field) {
        return field in data;  // Проверяет, существует ли поле в объекте
    }

</script>
<main>
    {#if $activeSection === 'telemetry'}
        <div id="telemetry_container" class="p-4 bg-white rounded-lg shadow-md">
            <h2 class="text-2xl font-semibold mb-6 text-center text-gray-800">Телеметрия</h2 >
            <h2 class="text-2xl font-semibold mb-6 text-center text-gray-800">Текущий режим: {getStageName(data.type)}</h2 >

            {#if isFieldPresent('stop') && data.stop === 1 }
                <Alert border color="red">
                    <InfoCircleSolid slot="icon" class="w-5 h-5" />
                    <span class="font-medium alig">Внимание!</span>
                    Отбор остановлен!
                </Alert>

            {/if}

            <div class="mt-8">
                <button
                        class="w-full text-lg font-semibold py-2 px-4 text-white bg-gray-500 rounded-lg focus:outline-none focus:ring-2 focus:ring-indigo-300"
                        on:click={() => paramsIsOpen = !paramsIsOpen}
                >
                    {paramsIsOpen ? 'Скрыть параметры режима' : 'Показать параметры режима'}
                </button>

                {#if paramsIsOpen}
                    <div  class="flex flex-wrap gap-6 justify-center ">
                        {#if isFieldPresent('countdown')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Время до окончания этапа</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.countdown}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('time')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Время отбора</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.time}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('open')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Время открытия клапана (сек)</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.open}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('period')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Период открытия клапана (сек)</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.period}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('hysteresis')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Гистерезис</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.hysteresis}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('tank_mmhg')}
                            <Card padding="md">
                                <div class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Давление в кубе (мм РС)</Label>
                                    <p class="text-xl font-semibold text-red-500">{data.tank_mmhg}</p>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('tp1_sap') || isFieldPresent('tp2_sap')}
                            <Card padding="md">
                                <div id="tp2_sap_container"  class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Температура (По давлению)</Label>
                                    <p class="text-xl font-semibold text-red-500">Т1: {data.tp1_sap}</p>
                                    <p class="text-xl font-semibold text-red-500">Т2: {data.tp2_sap}</p>
                                    <Popover class="w-64 text-sm font-light " title="tp2_sap" triggeredBy="#tp2_sap_container">
                                        Температура пересчитанная к нормальному атмосферному давлению.
                                    </Popover>
                                </div>
                            </Card>
                        {/if}

                        {#if isFieldPresent('v1') || isFieldPresent('v2') || isFieldPresent('v3') }
                            <Card padding="md">
                                <div id="valve_opening_time_container"  class="card shadow-sm p-4 rounded-lg">
                                    <Label class="font-medium text-lg mb-2 text-gray-600">Время открытия клапанов</Label>
                                    <p class="text-xl font-semibold text-red-500">Клапан 1: {data.v1}</p>
                                    <p class="text-xl font-semibold text-red-500">Клапан 2: {data.v2}</p>
                                    <p class="text-xl font-semibold text-red-500">Клапан 3: {data.v3}</p>
                                    <Popover class="w-64 text-sm font-light " title="tp2_sap" triggeredBy="#tp2_sap_container">
                                        Пересчитанная к нормальному атмосферному давлению температура ТД2.
                                    </Popover>
                                </div>
                            </Card>
                        {/if}


                    </div>
                {/if}
            </div>

            <!-- Спойлер для общих параметров -->
            <div class="mt-8">
                <button
                        class="w-full text-lg font-semibold py-2 px-4 text-white bg-gray-500 rounded-lg focus:outline-none focus:ring-2 focus:ring-indigo-300"
                        on:click={() => commonIsOpen = !commonIsOpen}
                >
                    {commonIsOpen ? 'Скрыть общие параметры' : 'Показать общие параметры'}
                </button>

                {#if commonIsOpen}
                    <div class="mt-4">
                        <div class="grid grid-cols-1 sm:grid-cols-2 gap-6">
                            <div class="card p-4 rounded-lg shadow-sm bg-gray-100">
                                <strong class="text-gray-700">MMHG:</strong> <span class="font-semibold">{data.common.mmhg}</span>
                            </div>
                            <div class="card p-4 rounded-lg shadow-sm bg-gray-100">
                                <strong class="text-gray-700">TP1:</strong> <span class="font-semibold">{data.common.tp1}</span>
                            </div>
                            <div class="card p-4 rounded-lg shadow-sm bg-gray-100">
                                <strong class="text-gray-700">TP2:</strong> <span class="font-semibold">{data.common.tp2}</span>
                            </div>
                            <div class="card p-4 rounded-lg shadow-sm bg-gray-100">
                                <strong class="text-gray-700">Реле:</strong>
                                <span class="font-semibold">
                                  {data.common.relay === 1 ? 'Включено' : 'Выключено'}
                                </span>
                            </div>
                            <div class="card p-4 rounded-lg shadow-sm bg-gray-100">
                                <strong class="text-gray-700">Сигнал:</strong>
                                <span class="font-semibold">
                                    {data.common.signal === 1 ? 'Включено' : 'Выключено'}
                                </span>
                            </div>
                        </div>
                    </div>
                {/if}
            </div>
        </div>
    {/if}
</main>

<style>
    .name {
       text-align: left;
    }
    .value {
        text-align: right;
        color: black;
        border: black;
    }
</style>