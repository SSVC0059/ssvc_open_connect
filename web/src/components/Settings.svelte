<script>
    import {Card} from "flowbite-svelte";

    import {activeSection} from "../scripts/store.js"
    export let data;

    // Группы для отображения
    const commonSettings = [
        { label: 'Звук', value: data?.settings?.sound  === 1 ? 'Включен' : 'Отключен'},
        { label: 'Барометр', value: data?.settings?.pressure  === 1 ? 'Включен' : 'Отключен'},
        { label: 'Реле инвертировано', value: data?.settings?.relay_inverted === 1 ? 'Да' : 'Нет' },
        { label: 'Автозапуск реле', value: data?.settings?.relay_autostart  === 1 ? 'Да' : 'Нет'},
        { label: 'Продолжать прерванный процесс', value: data?.settings?.autoresume  === 1 ? 'Да' : 'Нет'},
        { label: 'Автоматический переход между этапами', value: data?.settings?.auto_mode  === 1 ? 'Да' : 'Нет'},
        { label: 'Отложенный пуск, сек', value: data?.settings?.start_delay},
        { label: 'Пауза "на себя"', value: data?.settings?.hearts_pause=== 1 ? 'Включен' : 'Отключен'},

    ];

    const selection_rate = [
        { label: 'Головы:', class: "text-red-500"},
        { label: 'Время открытого клапана, период, сек', value: data?.settings?.heads[0] },
        { label: 'Период, сек', value: data?.settings?.heads[1]},
        { label: 'Время отбора голов, сек', value: data?.settings?.head_timer},


        { label: 'Тело:', class: "text-red-500"},
        { label: 'Время открытого клапана, период, сек', value: data?.settings?.hearts[0] },
        { label: 'Период, сек', value: data?.settings?.hearts[1] },
        { label: 'Гистерезис при отборе тела, °С', value: data?.settings?.hyst },
        { label: 'Декремент при отборе тела, %', value: data?.settings?.decrement },
        { label: 'Таймер фиксации температуры отбора тела, сек', value: data?.settings?.body_timer},
        { label: 'Температура ТД2 завершения отбора тела, °С', value: data?.settings?.hearts_finish_temp},


        { label: 'Xвосты', class: "text-red-500"},
        { label: "Время открытого клапана, сек", value: data?.settings?.tails[0]},
        { label: "Период, сек", value: data?.settings?.tails[1]},
        { label: "Температура завершения отбора хвостов, °С", value: data?.settings?.tail_temp},
    ];


    const valveCapacity = [
        // { label: 'Клапан 1', value: data?.settings?.valve_bandwidth?[0] },
        // { label: 'Клапан 2', value: data?.settings?.valve_bandwidth[1] },
        // { label: 'Клапан 3', value: data?.settings?.valve_bandwidth[2] },
    ];

    // const timingSettings = [
    //     { label: 'Таймер головы', value: data?.settings?.head_timer },
    //     { label: 'Таймер тела', value: data?.settings?.body_timer },
    //
    // ];

    // Переменная для активной вкладки
    let activeTab = 0;
</script>

<main>
    {#if $activeSection === 'GET_SETTINGS'}
        <div class="p-4">
            <div class="bg-white p-4 shadow-md rounded-lg">

                <!-- Вкладки -->
                <div class="flex space-x-4 mb-4">
                    <button
                            class="tab-button"
                            class:active={activeTab === 0}
                            on:click={() => activeTab = 0}
                    >
                        Общие настройки
                    </button>
                    <button
                            class="tab-button"
                            class:active={activeTab === 1}
                            on:click={() => activeTab = 1}
                    >
                        Настройки режимов
                    </button>
                    <button
                            class="tab-button"
                            class:active={activeTab === 2}
                            on:click={() => activeTab = 2}
                    >
                        Пропускная способность клапанов
                    </button>
<!--                    <button-->
<!--                            class="tab-button"-->
<!--                            class:active={activeTab === 3}-->
<!--                            on:click={() => activeTab = 3}-->
<!--                    >-->
<!--                        Время-->
<!--                    </button>-->
                </div>

                <!-- Контент вкладок -->
                <div>
                    {#if activeTab === 0}
                        <ul class="space-y-2">
                            {#each commonSettings as setting}
                                <li class="flex justify-between items-center py-2 border-b">
                                    <span class="font-medium {setting.class}" style={setting.style}>{setting.label}</span>
                                    <span class="font-medium text-right font-semibold text-neutral-900 ml-4  {setting.class}">{setting.value}</span>
                                </li>
                            {/each}
                        </ul>
                    {/if}

                    {#if activeTab === 1}
                        <ul class="space-y-2">
                            {#each selection_rate as setting}
                                <li class="flex justify-between items-center py-2 border-b">
                                    <span class="font-medium {setting.class}">{setting.label}</span>
                                    <span class="font-medium text-right font-semibold text-neutral-900 ml-4  {setting.class}" >{setting.value}</span>
                                </li>
                            {/each}
                        </ul>
                    {/if}

                    {#if activeTab === 2  }
                        <ul class="space-y-2">
                            {#each valveCapacity as setting}
                                <li class="flex justify-between items-center py-2 border-b">
                                    <span class="font-medium {setting.class}">{setting.label}</span>
                                    <span class="font-medium text-right font-semibold text-neutral-900 ml-4  {setting.class}">{setting.value}</span>
                                </li>
                            {/each}
                        </ul>
                    {/if}

                    <!--{#if activeTab === 3}-->
                    <!--    <ul class="space-y-2">-->
                    <!--        {#each timingSettings as setting}-->
                    <!--            <li class="flex justify-between items-center py-2 border-b">-->
                    <!--                <span class="font-medium {setting.class}">{setting.label}</span>-->
                    <!--                <span class="font-medium text-right font-semibold text-neutral-900 ml-4  {setting.class}">{setting.value}</span>-->
                    <!--            </li>-->
                    <!--        {/each}-->
                    <!--    </ul>-->
                    <!--{/if}-->
                </div>
            </div>
        </div>
<!--        <pre>{JSON.stringify(data, undefined, 2)}</pre>-->
    {/if}
</main>
<style>
    .tab-button {
        padding: 0.5rem 1rem;
        cursor: pointer;
        background-color: #f0f0f0;
        border: none;
        border-radius: 0.375rem;
        transition: background-color 0.3s;
        font-weight: 500;
    }

    .tab-button:hover {
        background-color: #e0e0e0;
    }

    .tab-button.active {
        background-color: #d0d0d0;
    }

    .value {

    }
</style>