<script>
    import { Navbar, NavBrand, NavLi, NavUl, NavHamburger } from 'flowbite-svelte';
    import { activeSection } from '../scripts/store.js';

</script>

<div id="naw_bar" class="relative px-8" style="width: 100%">
    <Navbar  class="px-2 sm:px-4 py-2.5  w-full z-20 top-0 start-0 border-b">
        <NavBrand>
            <span class="self-center whitespace-nowrap text-xl font-semibold dark:text-white">Open Connect</span>
        </NavBrand>
        <NavHamburger />
        <NavUl>
            <NavUl >
                <NavLi on:click={() => activeSection.set('telemetry')}>Телеметрия</NavLi>
                <NavLi on:click={() => activeSection.set('debug')}>Debug</NavLi>
<!--                <NavLi on:click={() => activeSection.set('ssvs_settings')}>Настройки SSVC</NavLi>-->
<!--                <NavLi>Настройки OpenConnect</NavLi>-->
<!--                <NavLi>О программе</NavLi>-->
            </NavUl>
        </NavUl>
    </Navbar>
</div>