<script>
    import DataHandler from "./components/DataHandler.svelte"
    import Telemetry  from "./components/Telemetry.svelte"

    let telemetryData;

    // Обработчик события dataReceived
    function handleDataReceived(event) {
        telemetryData = event.detail;
    }

</script>

<main>
    <h1>Отображение данных с сервера</h1>
    <DataHandler on:dataReceived={handleDataReceived}/>
    {#if telemetryData && telemetryData.type !== "response"}
        <Telemetry data={telemetryData} />
    {:else}
        <p>Нет данных для отображения.</p>
    {/if}

</main>

<style>
    main {
        font-family: Arial, sans-serif;
        text-align: center;
        padding: 2em;
    }
</style>