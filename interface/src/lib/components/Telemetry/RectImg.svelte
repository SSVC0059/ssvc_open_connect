<script>
	import { GridItem } from 'svelte-grid-extended';
	import RectImg from '$lib/assets/rect.png'

</script>
<GridItem x={6} y={1} w={2} h={19} class="item bg-gray-100">
	<div class="flex justify-center mt-2">
		<img src={RectImg} alt="Logo" class="h-full object-contain"/>
	</div>
</GridItem>