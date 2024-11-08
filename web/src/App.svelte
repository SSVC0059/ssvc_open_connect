<script>
    import './app.css';

    import { writable } from 'svelte/store';

    export const activeSection = writable('telemetry'); // Начальная секция — 'telemetry'

    import DataHandler from "$components/DataHandler.svelte"
    import NavBar from "$components/NavBar.svelte";
    import Telemetry  from "$components/Telemetry.svelte"
    import Debug from "$components/Debug.svelte";
    import Settings from "$components/Settings.svelte";

    let telemetryData;

    // Обработчик события dataReceived
    function handleDataReceived(event) {
        telemetryData = event.detail;
    }


</script>

<main>
    <DataHandler on:dataReceived={handleDataReceived}/>

    <NavBar/>
    {#if telemetryData }
        {#if telemetryData.type === "response"}
            <div>
                <Settings data={telemetryData}/>
            </div>
        {:else}
            <div>
                <Telemetry data={telemetryData}/>
            </div>
        {/if}
    {:else}
        <p>Нет данных для отображения.</p>
    {/if}

    <Debug data={telemetryData}/>

</main>
