<script lang="ts">
    import logo from '$lib/assets/logo.png';
    import Github from '~icons/tabler/brand-github';
    import Discord from '~icons/tabler/brand-discord';
    import Users from '~icons/tabler/users';
    import Settings from '~icons/tabler/settings';
    import Health from '~icons/tabler/stethoscope';
    import Update from '~icons/tabler/refresh-alert';
    import WiFi from '~icons/tabler/wifi';
    import Router from '~icons/tabler/router';
    import AP from '~icons/tabler/access-point';
    import Remote from '~icons/tabler/network';
    import Control from '~icons/tabler/adjustments';
    import DropletCog from '~icons/tabler/droplet-cog';
    import DeviceTabletStar from '~icons/tabler/device-tablet-star';
    import CapStraight from '~icons/tabler/cap-straight';
    import ClockEdit from '~icons/tabler/clock-edit';
    import Avatar from '~icons/tabler/user-circle';
    import Logout from '~icons/tabler/logout';
    import Copyright from '~icons/tabler/copyright';
    import MQTT from '~icons/tabler/topology-star-3';
    import NTP from '~icons/tabler/clock-check';
    import Telegram from '~icons/tabler/brand-telegram';
    import Metrics from '~icons/tabler/report-analytics';
    import Connected from '~icons/tabler/plug-connected';
    import Book from '~icons/tabler/book';
    import SsvcIcon from '~icons/mdi/snake';
    import Esp from '~icons/mdi/car-esp';
    import Bug from '~icons/tabler/bug';
    import Temperature from '~icons/tabler/temperature';
import { page } from '$app/state';
import { goto } from '$app/navigation';
    import { user } from '$lib/stores/user';
    import type { Component } from 'svelte';

    let { closeMenu } = $props();

    const github = { href: 'https://github.com/' + page.data.github, active: true };

    const discord = { href: '.', active: false };

    type menuItem = {
        title: string;
        icon: Component;
        href?: string;
        feature: boolean;
        active?: boolean;
        submenu?: subMenuItem[];
    };

    type subMenuItem = {
        title: string;
        icon: Component;
        href: string;
        feature: boolean;
        active?: boolean;
        submenu?: subMenuItem[];
    };

    let menuItems = $state([
        {
            title: 'SSVC',
            icon: SsvcIcon,
            href: '/oc',
            feature: true,
            submenu: [
                {
                    title: 'Телеметрия',
                    icon: Connected,
                    href: '/oc/telemetry',
                    feature: true
                },
                {
                    title: 'Настройки SSVC',
                    icon: Control,
                    href: '/oc/ssvc?tab=general',
                    feature: true,
                    submenu: [
                        {
                            title: 'Общие',
                            icon: Control,
                            href: '/oc/ssvc?tab=general',
                            feature: true
                        },
                        {
                            title: 'Клапаны',
                            icon: DropletCog,
                            href: '/oc/ssvc?tab=valve-bandwidth',
                            feature: true
                        },
                        {
                            title: 'Параметры отбора',
                            icon: ClockEdit,
                            href: '/oc/ssvc?tab=speed',
                            feature: true
                        },
                        {
                            title: 'Параллельный отбор',
                            icon: CapStraight,
                            href: '/oc/ssvc?tab=parallel',
                            feature: true
                        }
                    ]
                },
                {
                    title: 'Open Connect',
                    icon: Esp,
                    href: '/oc/settings',
                    feature: true,
                    submenu: [
                        {
                            title: 'Профили',
                            icon: DeviceTabletStar,
                            href: '/oc/settings?tab=profiles',
                            feature: true
                        },
                        {
                            title: 'Датчики температуры',
                            icon: Temperature,
                            href: '/oc/settings?tab=thermal',
                            feature: true
                        },
                        {
                            title: 'Telegram',
                            icon: Telegram,
                            href: '/oc/settings?tab=telegram_bot',
                            feature: true
                        }
                    ]
                }
            ]
        },
        {
            title: 'Network',
            icon: Remote,
            feature: page.data.features.mqtt || page.data.features.ntp,
            submenu: [
                {
                    title: 'MQTT',
                    icon: MQTT,
                    href: '/network?tab=mqtt',
                    feature: page.data.features.mqtt
                },
                {
                    title: 'NTP',
                    icon: NTP,
                    href: '/network?tab=ntp',
                    feature: page.data.features.ntp
                }
            ]
        },
        {
            title: 'WiFi',
            icon: WiFi,
            feature: true,
            submenu: [
                {
                    title: 'WiFi Статус',
                    icon: Router,
                    href: '/wifi?tab=sta',
                    feature: true
                },
                {
                    title: 'Точка доступа',
                    icon: AP,
                    href: '/wifi?tab=ap',
                    feature: true
                }
            ]
        },
        {
            title: 'Пользователи',
            icon: Users,
            href: '/user',
            feature: page.data.features.security && $user.admin
        },
        {
            title: 'Система',
            icon: Settings,
            feature: true,
            submenu: [
                {
                    title: 'Статус системы',
                    icon: Health,
                    href: '/system?tab=status',
                    feature: true
                },
                {
                    title: 'Системный монитор',
                    icon: Metrics,
                    href: '/system?tab=metrics',
                    feature: page.data.features.analytics
                },
                {
                    title: 'Core Dump',
                    icon: Bug,
                    href: '/system?tab=coredump',
                    feature: page.data.features.coredump
                },
                {
                    title: 'Обновление системы',
                    icon: Update,
                    href: '/system?tab=update',
                    feature:
                        (page.data.features.ota ||
                            page.data.features.upload_firmware ||
                            page.data.features.download_firmware) &&
                        (!page.data.features.security || $user.admin)
                }
            ]
        },
        {
            title: 'Документация',
            icon: Book,
            href: '/documentation',
            feature: true
        },
    ] as menuItem[]);

    function setActiveMenuItem(targetTitle: string) {
        menuItems.forEach((item) => {
            item.active = item.title === targetTitle;

            item.submenu?.forEach((subItem) => {
                subItem.active = subItem.title === targetTitle;

                subItem.submenu?.forEach((child) => {
                    child.active = child.title === targetTitle;

                    if (child.active) {
                        subItem.active = true;
                        item.active = true;
                    }
                });

                if (subItem.active) {
                    item.active = true;
                }
            });
        });
        closeMenu();
    }

    function openParentDefault(subItem: subMenuItem) {
        const firstChild = subItem.submenu && subItem.submenu[0];
        if (firstChild) {
            goto(firstChild.href);
            setActiveMenuItem(firstChild.title);
        } else if (subItem.href) {
            goto(subItem.href);
            setActiveMenuItem(subItem.title);
        }
    }

    function openRootDefault(item: menuItem) {
        // Специальный случай: корневой пункт SSVC всегда ведёт в телеметрию
        if (item.title === 'SSVC') {
            goto('/oc/telemetry', { replaceState: true });
            setActiveMenuItem('Телеметрия');
            return;
        }

        const firstChild = item.submenu && item.submenu[0];
        if (firstChild && firstChild.href) {
            goto(firstChild.href, { replaceState: true });
            setActiveMenuItem(firstChild.title);
        } else if (item.href) {
            goto(item.href, { replaceState: true });
            setActiveMenuItem(item.title);
        }
    }

    $effect(() => {
        setActiveMenuItem(page.data.title);
    });
</script>

<div class="bg-base-200 text-base-content flex h-full w-80 flex-col p-4">
    <!-- Sidebar content here -->
    <a
            href="/"
            class="rounded-box mb-4 flex items-center hover:scale-[1.02] active:scale-[0.98]"
            onclick={() => setActiveMenuItem('')}
    >
        <img src={logo} alt="Logo" class="max-h-12 max-w-12 h-auto w-auto object-contain" />
        <h1 class="px-4 text-2xl font-bold">{page.data.appName}</h1>
    </a>
    <ul class="menu w-full rounded-box menu-vertical flex-nowrap overflow-y-auto">
        {#each menuItems as menuItem, i (menuItem.title)}
            {#if menuItem.feature}
                <li>
                    {#if menuItem.submenu}
                        <details open={menuItem.submenu.some((subItem) => subItem.active)}>
                            <summary
                                    class="text-lg font-bold"
                                    onclick={() => openRootDefault(menuItem)}
                            >
                                <menuItem.icon class="h-6 w-6" />
                                {menuItem.title}
                            </summary>
                            <ul>
                                {#each menuItem.submenu as subMenuItem}
                                    {#if subMenuItem.feature}
                                        <li class="hover-bordered">
                                            {#if subMenuItem.submenu}
                                                <details open={subMenuItem.submenu.some((child) => child.active)}>
                                                    <summary
                                                            class="text-ml font-bold"
                                                            onclick={() => openParentDefault(subMenuItem)}
                                                    >
                                                        <subMenuItem.icon class="h-5 w-5" />
                                                        {subMenuItem.title}
                                                    </summary>
                                                    <ul>
                                                        {#each subMenuItem.submenu as child}
                                                            {#if child.feature}
                                                                <li class="hover-bordered">
                                                                    <a
                                                                            href={child.href}
                                                                            class:bg-base-100={child.active}
                                                                            class="text-ml font-bold"
                                                                            onclick={() => {
																				setActiveMenuItem(child.title);
																			}}><child.icon class="h-5 w-5" />{child.title}</a
                                                                    >
                                                                </li>
                                                            {/if}
                                                        {/each}
                                                    </ul>
                                                </details>
                                            {:else}
                                                <a
                                                        href={subMenuItem.href}
                                                        class:bg-base-100={subMenuItem.active}
                                                        class="text-ml font-bold"
                                                        onclick={() => {
															setActiveMenuItem(subMenuItem.title);
														}}><subMenuItem.icon class="h-5 w-5" />{subMenuItem.title}</a
                                                >
                                            {/if}
                                        </li>
                                    {/if}
                                {/each}
                            </ul>
                        </details>
                    {:else}
                        <a
                                href={menuItem.href}
                                class:bg-base-100={menuItem.active}
                                class="text-lg font-bold"
                                onclick={() => {
								setActiveMenuItem(menuItem.title);
							}}><menuItem.icon class="h-6 w-6" />{menuItem.title}</a
                        >
                    {/if}
                </li>
            {/if}
        {/each}
    </ul>

    <div class="flex-col"></div>
    <div class="grow"></div>

    {#if page.data.features.security}
        <div class="flex items-center">
            <Avatar class="h-8 w-8" />
            <span class="grow px-4 text-xl font-bold">{$user.username}</span>
            <!-- svelte-ignore a11y_click_events_have_key_events -->
            <!-- svelte-ignore a11y_no_static_element_interactions -->
            <div
                    class="btn btn-ghost"
                    onclick={() => {
					user.invalidate();
				}}
            >
                <Logout class="h-8 w-8 rotate-180" />
            </div>
        </div>
    {/if}

    <div class="divider my-0"></div>
    <div class="flex items-center">
        {#if github.active}
            <a href={github.href} class="btn btn-ghost" target="_blank" rel="noopener noreferrer"
            ><Github class="h-5 w-5" /></a
            >
        {/if}
        {#if discord.active}
            <a href={discord.href} class="btn btn-ghost" target="_blank" rel="noopener noreferrer"
            ><Discord class="h-5 w-5" /></a
            >
        {/if}
        <div class="inline-flex grow items-center justify-end text-sm">
            <Copyright class="h-4 w-4" /><span class="px-2">{page.data.copyright}</span>
        </div>
    </div>
</div>
