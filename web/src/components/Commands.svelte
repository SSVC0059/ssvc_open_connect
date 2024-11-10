<script>

import {Button, Modal} from "flowbite-svelte";

import {sendPostRequestSilent} from "../scripts/store.js";

let stopModal = false;
let pauseModal = false;
let resumeModal = false;
let nextModal = false;

</script>

<dib id="command" class="flex space-x-4 items-center container py-10 px-10 mx-0 min-w-full">
    <Button on:click={() => (stopModal = true)}>СТОП</Button>
    <Modal title="Подтверждение остановки" bind:open={stopModal} autoclose>
        <p class="text-base leading-relaxed text-gray-500 dark:text-gray-400">
            Вы действительно хотите прервать процесс?
        </p>
        <svelte:fragment slot="footer">
            <Button on:click={() => sendPostRequestSilent("STOP")}>Да, остановить</Button>
            <Button color="alternative">Отменить</Button>
        </svelte:fragment>
    </Modal>

    <Button on:click={() => (pauseModal = true)}>Пауза</Button>
    <Modal title="Terms of Service" bind:open={pauseModal} autoclose>
        <p class="text-base leading-relaxed text-gray-500 dark:text-gray-400">
            Вы действительно хотите остановить процесс?
        </p>
        <svelte:fragment slot="footer">
            <Button on:click={() => sendPostRequestSilent("PAUSE")}>Да, остановить</Button>
            <Button color="alternative">Отменить</Button>
        </svelte:fragment>
    </Modal>

    <Button on:click={() => (resumeModal = true)}>Продолжить</Button>
    <Modal title="Terms of Service" bind:open={resumeModal} autoclose>
        <p class="text-base leading-relaxed text-gray-500 dark:text-gray-400">
            Вы действительно хотите продолжить процесс?
        </p>
        <svelte:fragment slot="footer">
            <Button on:click={() => sendPostRequestSilent("RESUME")}>Да, остановить</Button>
            <Button color="alternative">Отменить</Button>
        </svelte:fragment>
    </Modal>

    <Button on:click={() => (nextModal = true)}>Следующий этап</Button>
    <Modal title="Terms of Service" bind:open={nextModal} autoclose>
        <p class="text-base leading-relaxed text-gray-500 dark:text-gray-400">
            Вы действительно хотите Перейти на следующий этап?
        </p>
        <svelte:fragment slot="footer">
            <Button on:click={() => sendPostRequestSilent("NEXT")}>Да, остановить</Button>
            <Button color="alternative">Отменить</Button>
        </svelte:fragment>
    </Modal>

</dib>