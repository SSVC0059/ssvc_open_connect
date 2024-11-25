<script lang="ts">
	import type { PageData } from './$types';
	import logo from '$lib/assets/logo.png';
	import { notifications } from '$lib/components/toasts/notifications';

	export let data: PageData;
</script>

<div class="hero bg-base-100 h-screen">
	<div class="card md:card-side bg-base-200 shadow-primary shadow-2xl">
		<figure class="bg-base-200"><img src={logo} alt="Logo" class="h-auto w-64" /></figure>
		<div class="card-body w-80">
			<h2 class="card-title text-center text-2xl">Добро пожаловать!</h2>
			<h2 class="card-title text-center text-2xl">SSVC Open Connect</h2>
			<a
				class="btn btn-primary"
				href="ssvc/telemetry/"
				on:click={() => notifications.success('Начнем!', 1000)}>SSVC Open Connect</a
			>
		</div>
	</div>
</div>
