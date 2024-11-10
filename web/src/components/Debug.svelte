
<script>
    import { activeSection } from '../scripts/store.js';
    export let data;
    export let ssvc_settings;

</script>

{#if $activeSection === 'debug'}
    <div>
        <pre>{JSON.stringify(data, undefined, 2)}</pre>

        <p> SSVC_SETTINGS</p>
        <pre>{JSON.stringify(ssvc_settings, undefined, 2)}</pre>
    </div>
{/if}