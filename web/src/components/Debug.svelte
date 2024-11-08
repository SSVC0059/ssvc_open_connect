
<script>
    import { activeSection } from '../scripts/store.js';
    export let data;

</script>

{#if $activeSection === 'debug'}
    <div>
        <pre>{JSON.stringify(data, undefined, 2)}</pre>
    </div>
{/if}