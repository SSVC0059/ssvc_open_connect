<script>
    import './app.css';

    import { writable } from 'svelte/store';

    export const activeSection = writable('telemetry'); // Начальная секция — 'telemetry'

    import DataHandler from "$components/DataHandler.svelte"
    import NavBar from "$components/NavBar.svelte";
    import Telemetry  from "$components/Telemetry.svelte"
    import Debug from "$components/Debug.svelte";
    import Settings from "$components/Settings.svelte";
    import {InfoCircleSolid} from "flowbite-svelte-icons";
    import {Alert} from "flowbite-svelte";
    import Commands from "$components/Commands.svelte";


    // Переменная для сохранения последнего сообщения с `type: "response"` и `request: "GET_SETTINGS"`
    export let ssvc_settings = null;
    export let command_responce = "{}";

    let telemetryData;

    // Обработчик события dataReceived
    function handleDataReceived(event) {
        telemetryData = event.detail;
        handleEvent(telemetryData);

    }

    // Функция для обработки сообщений EventSource
    function handleEvent(message) {
        // Проверка типа и запроса
        if (message.type === "response") {
            if (message.request === "GET_SETTINGS") {
                // Обновляем переменную, только если пришел такой же запрос
                ssvc_settings = message;
                console.log("Сохраненный ответ:", ssvc_settings);
            }else {
                command_responce = message;
            }
        }
    }

</script>

<main>
    <DataHandler on:dataReceived={handleDataReceived}/>

    <NavBar/>

    <Commands data={command_responce}/>

    {#if telemetryData }
        <div>
            <Telemetry data={telemetryData}/>
        </div>
    {:else }
        <Alert border color="red">
            <InfoCircleSolid slot="icon" class="w-5 h-5" />
            <span class="font-medium alig">Внимание!</span>
            Ожидание получения данных...
        </Alert>
    {/if}

    {#if ssvc_settings }
        <div>
            <Settings data={ssvc_settings}/>
        </div>
    {/if}



    <Debug data={telemetryData} ssvc_settings="{ssvc_settings}"/>

</main>
