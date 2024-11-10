<!-- DataDisplay.svelte -->
<script>
    import { createEventDispatcher, onMount } from "svelte";
    const dispatch = createEventDispatcher();

    // Хранение полученных данных

    onMount(() => {
        let url = '/events'
        const eventSource = new EventSource(url);

        eventSource.onmessage = (event) => {
            const parsedData = JSON.parse(event.data);
            dispatch("dataReceived", parsedData);
        };

        return () => eventSource.close();
    });
</script>

